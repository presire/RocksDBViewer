# RocksDB Durability Test Procedure

This document verifies two properties of the Qt/C++ application:

1. **Normal exit does not corrupt the database**
2. **Crash / power-loss scenarios do not corrupt the database and minimize recent data loss**

The procedure assumes RocksDB's `ldb` tool is available from your RocksDB build or install.

## Prerequisites

- Built `RocksDBViewer` binary
- Built `tools/create_sample_db.cpp` helper or an existing `sample-db/`
- `ldb` from the same RocksDB major version used to build the app
- Writable scratch directory such as `/tmp`

Example environment:

```bash
export ROCKSDB_ROOT=/path/to/rocksdb
export LD_LIBRARY_PATH="$ROCKSDB_ROOT/lib:$ROCKSDB_ROOT/lib64:$LD_LIBRARY_PATH"
export LDB_BIN="${ROCKSDB_ROOT}/tools/ldb"
```

If `ldb` is installed globally, set:

```bash
export LDB_BIN="$(command -v ldb)"
```

Verify it works:

```bash
"$LDB_BIN" --version
```

## 1. Build the sample DB generator

If you do not already have a fresh test database, build the helper:

```bash
g++ -std=c++20 tools/create_sample_db.cpp -I"$ROCKSDB_ROOT/include" -L"$ROCKSDB_ROOT/lib64" -lrocksdb -o /tmp/create_sample_db
```

Create a scratch DB:

```bash
rm -rf /tmp/rocksdb-durability-base
/tmp/create_sample_db /tmp/rocksdb-durability-base
```

Verify the DB is initially healthy:

```bash
"$LDB_BIN" --db=/tmp/rocksdb-durability-base scan > /tmp/rocksdb-base-scan.txt
```

## 2. Prepare isolated test copies

```bash
rm -rf /tmp/rocksdb-test-normal /tmp/rocksdb-test-kill /tmp/rocksdb-test-import
cp -a /tmp/rocksdb-durability-base /tmp/rocksdb-test-normal
cp -a /tmp/rocksdb-durability-base /tmp/rocksdb-test-kill
cp -a /tmp/rocksdb-durability-base /tmp/rocksdb-test-import
```

## 3. Scenario A — Normal exit must not corrupt the DB

1. Launch the app with the copied database:

   ```bash
   ./RocksDBViewer /tmp/rocksdb-test-normal
   ```

2. In the UI, perform all of the following:
   - Edit an existing JSON value and save it
   - Add one new key such as `durability:test:normal`
   - Delete one existing key
   - If the DB has multiple column families, repeat one edit in a non-default column family

3. Close the window normally.
4. Verify with `ldb`:

   ```bash
   "$LDB_BIN" --db=/tmp/rocksdb-test-normal scan > /tmp/rocksdb-normal-scan.txt
   ```

5. Reopen the same DB in the app and confirm the edited value persists.

### Pass criteria

- `ldb scan` exits successfully
- No messages like these appear:
  - `Corruption:`
  - `unsupported format_version`
  - `MANIFEST ... may be corrupted`
- The app can reopen the DB
- The saved values are present after reopen

## 4. Scenario B — kill -9 crash tolerance

This scenario checks that the DB remains readable after abrupt process death.

1. Launch the app:

   ```bash
   ./RocksDBViewer /tmp/rocksdb-test-kill
   ```

2. In the UI, perform these writes:
   - Save one edited key in the default column family
   - Save one newly added key such as `durability:test:kill`
   - If available, import a small JSON file with 10–20 keys

3. Find the app PID:

   ```bash
   pgrep -a RocksDBViewer
   ```

4. Force-kill it without closing the window:

   ```bash
   kill -9 <PID>
   ```

5. Verify the DB structure is still readable:

   ```bash
   "$LDB_BIN" --db=/tmp/rocksdb-test-kill scan > /tmp/rocksdb-kill-scan.txt
   ```

6. Reopen the DB in the app.

### Pass criteria

- `ldb scan` exits successfully
- The DB reopens in the app without repair
- No corruption messages are reported
- For single-entry saves, the most recent saved values should still be present
- For larger imports, all keys should normally survive; if not, record exactly which keys were lost

### Notes

- A crash test is mainly checking **corruption resistance**, not only value persistence.
- If values are lost, record them as durability failures.
- If the DB cannot be scanned or reopened, record it as corruption.

## 5. Scenario C — Import / clear / scan verification

1. Create a JSON file for import:

   ```bash
   cat > /tmp/rocksdb-import.json <<'EOF'
   {
     "import:test:001": {"name": "alpha", "seq": 1},
     "import:test:002": {"name": "beta", "seq": 2},
     "import:test:003": {"name": "gamma", "seq": 3}
   }
   EOF
   ```

2. Launch the app with `/tmp/rocksdb-test-import`.
3. Import `/tmp/rocksdb-import.json`.
4. Close the app normally.
5. Verify imported records:

   ```bash
   "$LDB_BIN" --db=/tmp/rocksdb-test-import scan > /tmp/rocksdb-import-scan.txt
   grep "import:test:" /tmp/rocksdb-import-scan.txt
   ```

6. Reopen the DB.
7. Run **Clear All Data** in one column family.
8. Close the app normally.
9. Run scan again and confirm the cleared keys are gone but the DB is still healthy:

   ```bash
   "$LDB_BIN" --db=/tmp/rocksdb-test-import scan > /tmp/rocksdb-import-after-clear.txt
   ```

### Pass criteria

- Imported keys appear in scan output
- Cleared keys disappear after clear + close
- `ldb scan` still succeeds after import and after clear
- The app can reopen the DB after each step

## 6. Optional deeper checks

If your `ldb` build supports them, also run:

```bash
"$LDB_BIN" --db=/tmp/rocksdb-test-normal checkconsistency
"$LDB_BIN" --db=/tmp/rocksdb-test-kill checkconsistency
"$LDB_BIN" --db=/tmp/rocksdb-test-import checkconsistency
```

If unsupported, plain `scan` plus reopen verification is acceptable.

## 7. Results template

| Scenario | DB Path | Operations | Exit Mode | `ldb scan` | Reopen | Data persisted | Notes |
|---|---|---|---|---|---|---|---|
| A | `/tmp/rocksdb-test-normal` | edit/add/delete | normal | pass/fail | pass/fail | yes/no | |
| B | `/tmp/rocksdb-test-kill` | edit/add/import | `kill -9` | pass/fail | pass/fail | yes/no | |
| C1 | `/tmp/rocksdb-test-import` | import | normal | pass/fail | pass/fail | yes/no | |
| C2 | `/tmp/rocksdb-test-import` | clear | normal | pass/fail | pass/fail | yes/no | |

## 8. Failure classification

- **Corruption failure**
  - `ldb scan` fails
  - reopen fails
  - corruption-related RocksDB errors appear

- **Durability failure**
  - DB remains readable
  - but expected saved keys are missing after close or crash

- **Pass**
  - DB remains readable
  - reopen succeeds
  - expected values persist for the tested operation
