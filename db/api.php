<?php
header('Content-Type: application/json; charset=utf-8');

$DB_HOST   = 'localhost';
$DB_NAME   = 'postuvital';
$DB_USER   = 'postuvital_app';
$DB_PASS   = 'CAMBIA_ESTA_CONTRASENA';   // la misma del SQL
$API_CLAVE = 'CAMBIA_ESTA_CLAVE';        // la misma que va en el ESP32
$ID_USUARIO_POR_DEFECTO = 1;             // dueño de los dispositivos nuevos

function responder($codigo, $datos) {
  http_response_code($codigo);
  echo json_encode($datos);
  exit;
}
function numero($v, $min, $max) {
  return (is_numeric($v) && $v >= $min && $v <= $max) ? (float)$v : null;
}

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') responder(405, ['error' => 'usa POST']);
$in = json_decode(file_get_contents('php://input'), true);
if (!is_array($in)) responder(400, ['error' => 'JSON invalido']);
if (!hash_equals($API_CLAVE, (string)($in['clave'] ?? ''))) responder(401, ['error' => 'clave incorrecta']);

$mac = strtoupper((string)($in['mac'] ?? ''));
if (!preg_match('/^([0-9A-F]{2}:){5}[0-9A-F]{2}$/', $mac)) responder(400, ['error' => 'MAC invalida']);

$bpm   = numero($in['bpm'] ?? null, 20, 250);
$spo2  = numero($in['spo2'] ?? null, 50, 100);
$temp  = numero($in['temp_c'] ?? null, 0, 60);
// acc_g: un golpe fuerte puede pasar de 20 g; se recorta en vez de perder la lectura
$acc   = (isset($in['acc_g']) && is_numeric($in['acc_g']) && $in['acc_g'] >= 0)
         ? min((float)$in['acc_g'], 20.0) : null;
$roll  = numero($in['roll'] ?? null, -180, 180);
$pitch = numero($in['pitch'] ?? null, -180, 180);
$fall  = empty($in['fall']) ? 0 : 1;
$inicio = !empty($in['inicio']);
if ($bpm === null && $spo2 === null && $temp === null && $acc === null) {
  responder(400, ['error' => 'sin datos validos']);
}

try {
  $pdo = new PDO("mysql:host=$DB_HOST;dbname=$DB_NAME;charset=utf8mb4", $DB_USER, $DB_PASS,
                 [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
  $pdo->exec("SET time_zone = '-06:00'");
  $pdo->beginTransaction();

  // 1) Dispositivo: si la MAC no existe, se registra
  $st = $pdo->prepare('SELECT id_device FROM device WHERE mac_address = ?');
  $st->execute([$mac]);
  $idDevice = $st->fetchColumn();
  if (!$idDevice) {
    $pdo->prepare('INSERT INTO device (id_user, name, mac_address) VALUES (?, ?, ?)')
        ->execute([$ID_USUARIO_POR_DEFECTO, 'ESP32 ' . $mac, $mac]);
    $idDevice = (int)$pdo->lastInsertId();
  }

  // 2) Sesión: una por cada encendido del ESP32
  if ($inicio) {
    $pdo->prepare('UPDATE `session` SET end_time = NOW() WHERE id_device = ? AND end_time IS NULL')
        ->execute([$idDevice]);
  }
  $st = $pdo->prepare('SELECT id_session FROM `session` WHERE id_device = ? AND end_time IS NULL
                       ORDER BY id_session DESC LIMIT 1');
  $st->execute([$idDevice]);
  $idSession = $st->fetchColumn();
  if (!$idSession) {
    $pdo->prepare('INSERT INTO `session` (id_device) VALUES (?)')->execute([$idDevice]);
    $idSession = (int)$pdo->lastInsertId();
  }

  // 3) Lecturas: variable => [valor, columna_id_en_alert, id_de_la_lectura]
  $lectura = [];
  if ($acc !== null) {
    $pdo->prepare('INSERT INTO motion_reading (id_session, acc_g, roll, pitch, fall_detected)
                   VALUES (?, ?, ?, ?, ?)')
        ->execute([$idSession, round($acc, 2), $roll, $pitch, $fall]);
    $idMotion = (int)$pdo->lastInsertId();
    $lectura['acc_g'] = [round($acc, 2), 'id_motion', $idMotion];
    $lectura['fall']  = [$fall, 'id_motion', $idMotion];
  }
  if ($bpm !== null || $spo2 !== null) {
    $b = $bpm  === null ? null : (int)round($bpm);
    $s = $spo2 === null ? null : (int)round($spo2);
    $pdo->prepare('INSERT INTO pulse_reading (id_session, bpm, spo2) VALUES (?, ?, ?)')
        ->execute([$idSession, $b, $s]);
    $idPulse = (int)$pdo->lastInsertId();
    if ($b !== null) $lectura['bpm']  = [$b, 'id_pulse', $idPulse];
    if ($s !== null) $lectura['spo2'] = [$s, 'id_pulse', $idPulse];
  }
  if ($temp !== null) {
    $t = round($temp, 2);
    $pdo->prepare('INSERT INTO temperature_reading (id_session, temp_c) VALUES (?, ?)')
        ->execute([$idSession, $t]);
    $lectura['temp_c'] = [$t, 'id_temp', (int)$pdo->lastInsertId()];
  }

  // 4) Alertas según los umbrales activos del usuario
  $mapa = [
    'acc_g'  => ['motion_reading', 'acc_g', 'id_motion'],
    'fall'   => ['motion_reading', 'fall_detected', 'id_motion'],
    'bpm'    => ['pulse_reading', 'bpm', 'id_pulse'],
    'spo2'   => ['pulse_reading', 'spo2', 'id_pulse'],
    'temp_c' => ['temperature_reading', 'temp_c', 'id_temp'],
  ];
  $st = $pdo->prepare('SELECT t.* FROM threshold t JOIN device d ON d.id_user = t.id_user
                       WHERE d.id_device = ? AND t.active = 1');
  $st->execute([$idDevice]);
  $nuevas = 0;

  foreach ($st->fetchAll(PDO::FETCH_ASSOC) as $u) {
    $var = $u['variable'];
    if (!isset($lectura[$var])) continue;
    [$valor, $colId, $idLectura] = $lectura[$var];
    $min = $u['min_value'] === null ? -1e9 : (float)$u['min_value'];
    $max = $u['max_value'] === null ?  1e9 : (float)$u['max_value'];
    $fuera = ($valor < $min || $valor > $max);

    $ab = $pdo->prepare('SELECT id_alert FROM alert WHERE id_threshold = ? AND end_time IS NULL
                         ORDER BY id_alert DESC LIMIT 1');
    $ab->execute([$u['id_threshold']]);
    $idAbierta = $ab->fetchColumn();

    if (!$fuera) {                                   // volvió al rango: se cierra la alerta
      if ($idAbierta) {
        $pdo->prepare('UPDATE alert SET end_time = NOW() WHERE id_alert = ?')->execute([$idAbierta]);
      }
      continue;
    }
    if ($idAbierta) continue;                        // ya hay una alerta abierta

    // ¿Cuántos segundos lleva fuera de rango sin interrupción?
    [$tabla, $col, $pk] = $mapa[$var];
    $q = $pdo->prepare("SELECT TIMESTAMPDIFF(SECOND, MIN(ts), NOW()) FROM $tabla
                        WHERE id_session = :s AND $col IS NOT NULL
                          AND $pk > COALESCE((SELECT MAX($pk) FROM $tabla
                                              WHERE id_session = :s2 AND $col BETWEEN :mn AND :mx), 0)");
    $q->execute([':s' => $idSession, ':s2' => $idSession, ':mn' => $min, ':mx' => $max]);
    $seg = $q->fetchColumn();

    if ($seg !== null && (int)$seg >= (int)$u['duration_s']) {
      $pdo->prepare("INSERT INTO alert (id_threshold, $colId, `value`, start_time)
                     VALUES (?, ?, ?, NOW() - INTERVAL ? SECOND)")
          ->execute([$u['id_threshold'], $idLectura, $valor, (int)$seg]);
      $nuevas++;
    }
  }

  $pdo->commit();
  responder(200, ['ok' => true, 'sesion' => (int)$idSession, 'alertas_nuevas' => $nuevas]);
} catch (Throwable $e) {
  if (isset($pdo) && $pdo->inTransaction()) $pdo->rollBack();
  responder(500, ['error' => 'error del servidor']);
}
