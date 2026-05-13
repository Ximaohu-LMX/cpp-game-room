CREATE DATABASE IF NOT EXISTS game DEFAULT CHARACTER SET utf8mb4;
USE game;

CREATE TABLE IF NOT EXISTS player (
    player_id BIGINT PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    score INT DEFAULT 1000,
    win_count INT DEFAULT 0,
    lose_count INT DEFAULT 0,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS battle (
    battle_id BIGINT NOT NULL,
    room_id BIGINT NOT NULL,
    winner_id BIGINT NOT NULL,
    start_time TIMESTAMP NULL,
    end_time TIMESTAMP NULL,
    result_json TEXT,
    PRIMARY KEY (battle_id),
    KEY idx_battle_room (room_id),
    KEY idx_battle_winner (winner_id)
);

CREATE TABLE IF NOT EXISTS battle_player_result (
    result_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    battle_id BIGINT NOT NULL,
    player_id BIGINT NOT NULL,
    result VARCHAR(16) NOT NULL,
    score_delta INT NOT NULL,
    UNIQUE KEY uniq_battle_player_result (battle_id, player_id),
    KEY idx_battle_player (player_id)
);

CREATE TABLE IF NOT EXISTS settlement_log (
    settlement_id BIGINT PRIMARY KEY,
    battle_id BIGINT NOT NULL,
    player_id BIGINT NOT NULL,
    score_delta INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uniq_battle_player (battle_id, player_id)
);
