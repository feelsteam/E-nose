-- ============================================================
-- SmellGood E-Nose — 資料庫初始化腳本
-- 執行方式：mysql -u root -p < setup_db.sql
-- ============================================================

CREATE DATABASE IF NOT EXISTS smellgood
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE smellgood;

-- 受試者基本資料
CREATE TABLE IF NOT EXISTS `human` (
    `subject_id`  VARCHAR(64)  NOT NULL,
    `gender`      VARCHAR(10)  DEFAULT NULL,
    `age`         INT          DEFAULT NULL,
    `bmi`         FLOAT        DEFAULT NULL,
    `created_at`  DATETIME     DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`subject_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 每次量測的丙酮值
CREATE TABLE IF NOT EXISTS `value` (
    `id`          INT          NOT NULL AUTO_INCREMENT,
    `subject_id`  VARCHAR(64)  DEFAULT NULL,
    `acetone_ppm` FLOAT        DEFAULT NULL,
    `meal_state`  VARCHAR(20)  DEFAULT NULL,
    `uploaded_at` DATETIME     DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 每次完整預測的結果
CREATE TABLE IF NOT EXISTS `ans` (
    `id`             INT    NOT NULL AUTO_INCREMENT,
    `subject_id`     VARCHAR(64) DEFAULT NULL,
    `prediction`     INT         DEFAULT NULL,
    `confidence`     FLOAT       DEFAULT NULL,
    `acetone_max`    FLOAT       DEFAULT NULL,
    `acetone_slope`  FLOAT       DEFAULT NULL,
    `created_at`     DATETIME    DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

SELECT 'Database smellgood initialized successfully.' AS status;
