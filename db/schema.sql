DROP DATABASE IF EXISTS postuvital;
CREATE DATABASE postuvital CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE postuvital;

-- ---------- USER ----------
CREATE TABLE `user` (
  id_user    INT UNSIGNED NOT NULL AUTO_INCREMENT,
  name       VARCHAR(80)  NOT NULL,
  created_at TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id_user)
) ENGINE=InnoDB;

-- ---------- DEVICE (USER 1:N DEVICE, "owns") ----------
CREATE TABLE device (
  id_device   INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_user     INT UNSIGNED NOT NULL,
  name        VARCHAR(60)  NOT NULL,
  mac_address CHAR(17)     NOT NULL,
  mqtt_topic  VARCHAR(120) NULL,
  created_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id_device),
  UNIQUE KEY uq_device_mac (mac_address),
  UNIQUE KEY uq_device_topic (mqtt_topic),
  KEY idx_device_user (id_user),
  CONSTRAINT fk_device_user FOREIGN KEY (id_user)
    REFERENCES `user` (id_user) ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB;

-- ---------- CALIBRATION (DEVICE 1:N CALIBRATION, "has") ----------
CREATE TABLE calibration (
  id_calibration INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_device      INT UNSIGNED NOT NULL,
  ref_angle      DECIMAL(6,2) NOT NULL,
  calibrated_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id_calibration),
  KEY idx_calibration_device (id_device, calibrated_at),
  CONSTRAINT fk_calibration_device FOREIGN KEY (id_device)
    REFERENCES device (id_device) ON UPDATE CASCADE ON DELETE CASCADE
) ENGINE=InnoDB;

-- ---------- SESSION (DEVICE 1:N SESSION, "records") ----------
CREATE TABLE `session` (
  id_session INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_device  INT UNSIGNED NOT NULL,
  start_time DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  end_time   DATETIME     NULL,
  notes      VARCHAR(255) NULL,
  PRIMARY KEY (id_session),
  KEY idx_session_device (id_device, start_time),
  CONSTRAINT fk_session_device FOREIGN KEY (id_device)
    REFERENCES device (id_device) ON UPDATE CASCADE ON DELETE RESTRICT,
  CONSTRAINT chk_session_time CHECK (end_time IS NULL OR end_time >= start_time)
) ENGINE=InnoDB;

-- ---------- THRESHOLD (USER 1:N THRESHOLD, "defines") ----------
CREATE TABLE threshold (
  id_threshold INT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_user      INT UNSIGNED NOT NULL,
  `variable`   ENUM('acc_g','fall','bpm','spo2','temp_c') NOT NULL,
  min_value    DECIMAL(8,2) NULL,
  max_value    DECIMAL(8,2) NULL,
  duration_s   SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  active       TINYINT(1)   NOT NULL DEFAULT 1,
  PRIMARY KEY (id_threshold),
  KEY idx_threshold_user (id_user, active),
  CONSTRAINT fk_threshold_user FOREIGN KEY (id_user)
    REFERENCES `user` (id_user) ON UPDATE CASCADE ON DELETE CASCADE,
  CONSTRAINT chk_threshold_limite CHECK (min_value IS NOT NULL OR max_value IS NOT NULL),
  CONSTRAINT chk_threshold_orden CHECK (min_value IS NULL OR max_value IS NULL OR min_value <= max_value)
) ENGINE=InnoDB;

-- ---------- Lecturas (SESSION 1:N cada tabla, "contains") ----------
CREATE TABLE motion_reading (
  id_motion     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_session    INT UNSIGNED    NOT NULL,
  ts            DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
  acc_g         DECIMAL(5,2)    NOT NULL,
  roll          DECIMAL(6,2)    NULL,
  pitch         DECIMAL(6,2)    NULL,
  fall_detected TINYINT(1)      NOT NULL DEFAULT 0,
  PRIMARY KEY (id_motion),
  KEY idx_motion_session_ts (id_session, ts),
  CONSTRAINT fk_motion_session FOREIGN KEY (id_session)
    REFERENCES `session` (id_session) ON UPDATE CASCADE ON DELETE CASCADE,
  CONSTRAINT chk_motion_acc  CHECK (acc_g BETWEEN 0 AND 20),
  CONSTRAINT chk_motion_fall CHECK (fall_detected IN (0, 1))
) ENGINE=InnoDB;

CREATE TABLE pulse_reading (
  id_pulse   BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_session INT UNSIGNED    NOT NULL,
  ts         DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
  bpm        SMALLINT UNSIGNED NULL,
  spo2       TINYINT UNSIGNED  NULL,
  PRIMARY KEY (id_pulse),
  KEY idx_pulse_session_ts (id_session, ts),
  CONSTRAINT fk_pulse_session FOREIGN KEY (id_session)
    REFERENCES `session` (id_session) ON UPDATE CASCADE ON DELETE CASCADE,
  CONSTRAINT chk_pulse_algo CHECK (bpm IS NOT NULL OR spo2 IS NOT NULL),
  CONSTRAINT chk_pulse_bpm  CHECK (bpm IS NULL OR bpm BETWEEN 20 AND 250),
  CONSTRAINT chk_pulse_spo2 CHECK (spo2 IS NULL OR spo2 BETWEEN 50 AND 100)
) ENGINE=InnoDB;

CREATE TABLE temperature_reading (
  id_temp    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_session INT UNSIGNED    NOT NULL,
  ts         DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
  temp_c     DECIMAL(5,2)    NOT NULL,
  PRIMARY KEY (id_temp),
  KEY idx_temp_session_ts (id_session, ts),
  CONSTRAINT fk_temp_session FOREIGN KEY (id_session)
    REFERENCES `session` (id_session) ON UPDATE CASCADE ON DELETE CASCADE,
  CONSTRAINT chk_temp_rango CHECK (temp_c BETWEEN 0 AND 60)
) ENGINE=InnoDB;

-- ---------- ALERT (THRESHOLD 1:N ALERT, "triggers"; lectura 1:N ALERT, "generates") ----------
CREATE TABLE alert (
  id_alert     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  id_threshold INT UNSIGNED    NOT NULL,
  id_motion    BIGINT UNSIGNED NULL,
  id_pulse     BIGINT UNSIGNED NULL,
  id_temp      BIGINT UNSIGNED NULL,
  `value`      DECIMAL(8,2)    NOT NULL,
  start_time   DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
  end_time     DATETIME        NULL,
  acknowledged TINYINT(1)      NOT NULL DEFAULT 0,
  PRIMARY KEY (id_alert),
  KEY idx_alert_threshold (id_threshold, end_time),
  CONSTRAINT fk_alert_threshold FOREIGN KEY (id_threshold)
    REFERENCES threshold (id_threshold) ON UPDATE CASCADE ON DELETE RESTRICT,
  CONSTRAINT fk_alert_motion FOREIGN KEY (id_motion) REFERENCES motion_reading (id_motion),
  CONSTRAINT fk_alert_pulse  FOREIGN KEY (id_pulse)  REFERENCES pulse_reading (id_pulse),
  CONSTRAINT fk_alert_temp   FOREIGN KEY (id_temp)   REFERENCES temperature_reading (id_temp),
  -- cada alerta sale de exactamente una lectura
  CONSTRAINT chk_alert_una_lectura CHECK (
    (id_motion IS NOT NULL) + (id_pulse IS NOT NULL) + (id_temp IS NOT NULL) = 1),
  CONSTRAINT chk_alert_tiempo CHECK (end_time IS NULL OR end_time >= start_time)
) ENGINE=InnoDB;

-- ---------- Vista para leer las alertas completas ----------
CREATE OR REPLACE VIEW v_alert_detail AS
SELECT a.id_alert, u.name AS user_name, d.name AS device_name,
       t.`variable`, a.`value`, t.min_value, t.max_value, t.duration_s,
       a.start_time, a.end_time, a.acknowledged
FROM alert a
JOIN threshold t ON t.id_threshold = a.id_threshold
JOIN `user` u    ON u.id_user = t.id_user
LEFT JOIN motion_reading mo      ON mo.id_motion = a.id_motion
LEFT JOIN pulse_reading pu       ON pu.id_pulse = a.id_pulse
LEFT JOIN temperature_reading te ON te.id_temp = a.id_temp
LEFT JOIN `session` s ON s.id_session = COALESCE(mo.id_session, pu.id_session, te.id_session)
LEFT JOIN device d    ON d.id_device = s.id_device;

-- ---------- Datos iniciales ----------
INSERT INTO `user` (name) VALUES ('Usuario de prueba');

-- Umbrales de EJEMPLO: el equipo debe definir los valores reales.
-- 'fall': el valor es 1 cuando se detecta caida, y max_value = 0 hace que dispare la alerta.
INSERT INTO threshold (id_user, `variable`, min_value, max_value, duration_s) VALUES
  (1, 'fall',   NULL, 0,    0),
  (1, 'bpm',    50,   120,  30),
  (1, 'spo2',   90,   NULL, 10),
  (1, 'temp_c', NULL, 38,   30);

-- ---------- Usuario de la aplicación (cambia la contraseña) ----------
CREATE USER IF NOT EXISTS 'postuvital_app'@'localhost' IDENTIFIED BY 'CAMBIA_ESTA_CONTRASENA';
ALTER USER 'postuvital_app'@'localhost' IDENTIFIED BY 'CAMBIA_ESTA_CONTRASENA';
GRANT SELECT, INSERT, UPDATE ON postuvital.* TO 'postuvital_app'@'localhost';
FLUSH PRIVILEGES;
