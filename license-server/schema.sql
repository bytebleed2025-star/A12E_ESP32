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
