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
