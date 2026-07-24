-- MCU Skills MCP Service — MySQL schema (utf8mb4)
-- 服务启动时也会 SQLAlchemy create_all；本文件便于 DBA 预建库表。

CREATE DATABASE IF NOT EXISTS mcu_skills
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE mcu_skills;

CREATE TABLE IF NOT EXISTS api_keys (
  id INT NOT NULL AUTO_INCREMENT,
  key_id VARCHAR(32) NOT NULL,
  secret_hash VARCHAR(64) NOT NULL,
  name VARCHAR(128) NOT NULL DEFAULT '',
  role VARCHAR(16) NOT NULL DEFAULT 'user',
  enabled TINYINT(1) NOT NULL DEFAULT 1,
  created_at DATETIME(6) NULL,
  last_used_at DATETIME(6) NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uq_api_keys_key_id (key_id),
  KEY ix_api_keys_key_id (key_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS documents (
  id INT NOT NULL AUTO_INCREMENT,
  doc_id VARCHAR(64) NOT NULL,
  filename VARCHAR(512) NOT NULL,
  rel_path VARCHAR(1024) NOT NULL,
  content_hash VARCHAR(64) NOT NULL DEFAULT '',
  size_bytes INT NOT NULL DEFAULT 0,
  source VARCHAR(32) NOT NULL DEFAULT 'upload',
  created_at DATETIME(6) NULL,
  indexed_at DATETIME(6) NULL,
  PRIMARY KEY (id),
  UNIQUE KEY uq_documents_doc_id (doc_id),
  KEY ix_documents_doc_id (doc_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS chunks (
  id INT NOT NULL AUTO_INCREMENT,
  doc_id VARCHAR(64) NOT NULL,
  chunk_index INT NOT NULL DEFAULT 0,
  source VARCHAR(32) NOT NULL DEFAULT 'upload',
  uri VARCHAR(1024) NOT NULL DEFAULT '',
  title VARCHAR(512) NOT NULL DEFAULT '',
  text MEDIUMTEXT NOT NULL,
  content_hash VARCHAR(64) NOT NULL DEFAULT '',
  PRIMARY KEY (id),
  UNIQUE KEY uq_chunk_doc_index (doc_id, chunk_index),
  KEY ix_chunks_doc_id (doc_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS jobs (
  id INT NOT NULL AUTO_INCREMENT,
  job_type VARCHAR(32) NOT NULL DEFAULT 'reindex',
  status VARCHAR(16) NOT NULL DEFAULT 'pending',
  message LONGTEXT NOT NULL,
  created_at DATETIME(6) NULL,
  started_at DATETIME(6) NULL,
  finished_at DATETIME(6) NULL,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
