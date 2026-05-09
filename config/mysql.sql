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

CREATE TABLE IF NOT EXISTS battle_record (
    battle_id BIGINT PRIMARY KEY,
    room_id BIGINT NOT NULL,
    winner_id BIGINT NOT NULL,
    loser_id BIGINT NOT NULL,
    start_time TIMESTAMP NULL,
    end_time TIMESTAMP NULL,
    result_json TEXT
);

CREATE TABLE IF NOT EXISTS settlement_log (
    settlement_id BIGINT PRIMARY KEY,
    battle_id BIGINT NOT NULL,
    player_id BIGINT NOT NULL,
    score_delta INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uniq_battle_player (battle_id, player_id)
);

