USE reality;

-- -------------------------------------------------------------
-- Static World Objects Table
-- Backing store for ~2.88M MegaCity buildings, collision obstacles, and props
-- -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `static_world_objects` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `metrId` SMALLINT UNSIGNED NOT NULL COMMENT 'District ID: 1=Slums, 2=Downtown, etc',
  `sectorId` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `mxoId` VARCHAR(16) NOT NULL COMMENT 'MXO Hex ID',
  `staticId` VARCHAR(16) NOT NULL COMMENT 'Asset Hex ID',
  `type` VARCHAR(16) NOT NULL COMMENT 'Object Type Hex',
  `exterior` TINYINT(1) NOT NULL DEFAULT 1 COMMENT '1=Exterior, 0=Interior',
  `x` DOUBLE NOT NULL,
  `y` DOUBLE NOT NULL,
  `z` DOUBLE NOT NULL,
  `rot` DOUBLE NOT NULL DEFAULT 0,
  `quat` VARCHAR(64) DEFAULT NULL,
  PRIMARY KEY (`id`),
  INDEX `idx_metr_exterior` (`metrId`, `exterior`),
  INDEX `idx_spatial` (`x`, `z`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -------------------------------------------------------------
-- NPC Spawns Table
-- Backing store for 14,482 MegaCity world NPC spawns
-- -------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `npc_spawns` (
  `spawnId` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `district` VARCHAR(32) NOT NULL,
  `name` VARCHAR(64) NOT NULL,
  `level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `health` INT UNSIGNED NOT NULL DEFAULT 100,
  `innerStrength` INT UNSIGNED NOT NULL DEFAULT 100,
  `rsiHex` VARCHAR(32) NOT NULL DEFAULT '',
  `weaponHex` VARCHAR(32) DEFAULT NULL,
  `x` DOUBLE NOT NULL,
  `y` DOUBLE NOT NULL,
  `z` DOUBLE NOT NULL,
  `rot` DOUBLE NOT NULL DEFAULT 0,
  `faction` VARCHAR(32) NOT NULL DEFAULT 'Civilian',
  `isHostile` TINYINT(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`spawnId`),
  INDEX `idx_district` (`district`),
  INDEX `idx_faction` (`faction`),
  INDEX `idx_spatial` (`x`, `z`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
