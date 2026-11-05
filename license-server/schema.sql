-- A12E licensing orders
-- Run: wrangler d1 execute a12e-license --remote --file=./schema.sql

CREATE TABLE IF NOT EXISTS orders (
  id          TEXT PRIMARY KEY,
  device_id   TEXT NOT NULL,
  contact     TEXT NOT NULL DEFAULT '',
  note        TEXT NOT NULL DEFAULT '',
  status      TEXT NOT NULL DEFAULT 'pending', -- pending | paid | cancelled
  amount      TEXT NOT NULL DEFAULT '',
  license_key TEXT,
  created_at  INTEGER NOT NULL,
  paid_at     INTEGER
);

CREATE INDEX IF NOT EXISTS idx_orders_device  ON orders(device_id);
CREATE INDEX IF NOT EXISTS idx_orders_status  ON orders(status);
CREATE INDEX IF NOT EXISTS idx_orders_created ON orders(created_at);

-- One-time trial tokens. A Device ID can claim only once; erasing the device
-- NVS does not allow a second claim. Support can delete a row for RMA.
CREATE TABLE IF NOT EXISTS trials (
  device_id   TEXT PRIMARY KEY,
  token       TEXT NOT NULL,
  seconds     INTEGER NOT NULL,
  contact     TEXT NOT NULL DEFAULT '',
  created_at  INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_trials_created ON trials(created_at);
