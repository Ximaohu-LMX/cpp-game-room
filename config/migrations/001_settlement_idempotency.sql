-- 已存在的数据库执行一次；新建数据库直接使用 config/mysql.sql。
-- 不尝试修复旧实现可能遗留的部分结算数据。
ALTER TABLE player ENGINE = InnoDB;
ALTER TABLE battle
    ENGINE = InnoDB,
    MODIFY COLUMN battle_id BIGINT NOT NULL AUTO_INCREMENT,
    ADD COLUMN settled TINYINT NOT NULL DEFAULT 0;
ALTER TABLE battle_player_result ENGINE = InnoDB;
ALTER TABLE settlement_log
    ENGINE = InnoDB,
    MODIFY COLUMN settlement_id BIGINT NOT NULL AUTO_INCREMENT;
