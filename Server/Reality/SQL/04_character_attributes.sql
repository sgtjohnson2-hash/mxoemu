-- Character primary attributes (launcher attribute editor / RPC 0x80b2, 0x80b6) and the
-- info allocation counter. These were added on the live database by hand; this makes a fresh
-- database match it. Definitions copied from the live server (information_schema, 2026-10-14).
ALTER TABLE `characters`
  ADD COLUMN IF NOT EXISTS `belief` int(10) unsigned NOT NULL DEFAULT 8,
  ADD COLUMN IF NOT EXISTS `focus` int(10) unsigned NOT NULL DEFAULT 8,
  ADD COLUMN IF NOT EXISTS `perception` int(10) unsigned NOT NULL DEFAULT 8,
  ADD COLUMN IF NOT EXISTS `reason` int(10) unsigned NOT NULL DEFAULT 8,
  ADD COLUMN IF NOT EXISTS `vitality` int(10) unsigned NOT NULL DEFAULT 8,
  ADD COLUMN IF NOT EXISTS `info_allocation` bigint(20) unsigned NOT NULL DEFAULT 0;
