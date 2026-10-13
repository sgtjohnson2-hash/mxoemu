USE reality;

CREATE TABLE IF NOT EXISTS `ai_ltm` (
  `botId` INT,
  `targetId` INT,
  `trustScore` FLOAT,
  `dangerScore` FLOAT,
  PRIMARY KEY (`botId`, `targetId`)
);

CREATE TABLE IF NOT EXISTS `hardline_lockers` (
  `lockerId` BIGINT AUTO_INCREMENT PRIMARY KEY,
  `charId` BIGINT NOT NULL,
  `hardlineId` INT NOT NULL DEFAULT 0,
  `slot` INT NOT NULL,
  `templateId` INT NOT NULL,
  `quantity` INT NOT NULL DEFAULT 1,
  `item_metadata` TEXT DEFAULT NULL,
  `timeStored` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX (`charId`),
  INDEX (`hardlineId`)
);

CREATE TABLE IF NOT EXISTS `trade_listings` (
  `listingId` BIGINT AUTO_INCREMENT PRIMARY KEY,
  `sellerId` BIGINT NOT NULL,
  `templateId` INT NOT NULL,
  `infoPrice` INT NOT NULL,
  `quantity` INT NOT NULL DEFAULT 1,
  `isActive` TINYINT NOT NULL DEFAULT 1,
  `timeListed` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX (`sellerId`),
  INDEX (`isActive`)
);

CREATE TABLE IF NOT EXISTS `code_fragments` (
  `fragmentId` BIGINT AUTO_INCREMENT PRIMARY KEY,
  `charId` BIGINT NOT NULL,
  `fragmentType` INT NOT NULL,
  `sequenceData` VARCHAR(256) NOT NULL,
  `quality` INT NOT NULL DEFAULT 1,
  `timeExtracted` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  INDEX (`charId`)
);

CREATE TABLE IF NOT EXISTS `character_missions` (
  `charId` BIGINT(30) UNSIGNED NOT NULL,
  `missionId` INT UNSIGNED NOT NULL,
  `currentObjective` INT UNSIGNED NOT NULL DEFAULT 0,
  `state` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `completedAt` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`charId`, `missionId`),
  INDEX (`charId`),
  INDEX (`state`)
);

CREATE TABLE IF NOT EXISTS `bug_reports` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `char_id` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `handle` VARCHAR(64) NOT NULL DEFAULT '',
  `category` VARCHAR(32) NOT NULL DEFAULT 'GENERAL',
  `description` TEXT NOT NULL,
  `district` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `x` DOUBLE NOT NULL DEFAULT 0,
  `y` DOUBLE NOT NULL DEFAULT 0,
  `z` DOUBLE NOT NULL DEFAULT 0,
  `rot` DOUBLE NOT NULL DEFAULT 0,
  `target_go_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `fighting_style` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `hp` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `max_hp` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `status` VARCHAR(16) NOT NULL DEFAULT 'OPEN',
  `admin_notes` TEXT DEFAULT NULL,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  INDEX `idx_char_id` (`char_id`),
  INDEX `idx_status` (`status`),
  INDEX `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

