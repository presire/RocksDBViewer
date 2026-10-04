#!/usr/bin/env bash

# =============================================================================
# @file run_durability_test.sh
# @brief 書き込み耐久性 (Issue #2) のエンドツーエンド回帰テストを実行する
#
# Issue #2で報告された耐久性パイプライン全体を検証する
#
# 処理の流れ
#   0. ldbを探索する (任意)
#   1. テスト有効で、CMakeを構成する
#   2. create_sample_db (テスト用DB生成ツール) と、test_rocksdbbackend (QTest) をビルドする
#   3. QTestを実行し、編集後も、OPTIONSが保持されることを検証する
#   4. ldbが利用可能な場合のみ、生成したDBをスキャンし、古いツールでもSSTを読めることを確認する
#
# ldbが見つからない場合は、手順4をスキップして正常終了する
# 自動検証の主体は、手順3のQTestであり、常に実行される
#
# 使い方
#   ./tests/run_durability_test.sh [build_dir]
#   [build_dir] : ビルドディレクトリ(省略時はDebug)
#
# 環境変数
#   ROCKSDB_ROOT : RocksDBのインストール先 (任意、tools/ldbを探索する)
#                  未設定の場合は、PATH上のldbのみを探索する
#   LDB_BIN      : ldbバイナリのパス (任意、自動検出より優先される)
#                  未設定の場合は、ROCKSDB_ROOTとPATHから自動検出する
#
# 終了コード
#   0 : すべての検証に成功、またはldbが無いため、ldbスキャンをスキップした
#   1 : CMake構成・ビルド・ldbスキャンのいずれかに失敗した
#   その他 : QTestが失敗した場合は、set -eにより、test_rocksdbbackendの終了コードをそのまま返す
# =============================================================================
set -euo pipefail

# set -eはコマンドの失敗時、set -uは未定義変数の参照時、pipefailはパイプ途中の失敗時に、即座に終了させる指定
# 検証の失敗を見逃さないために付ける

# --- パス・設定 ---------------------------------------------------------------
# 呼び出し元のカレントディレクトリに依存しないよう、スクリプト自身の位置から求める
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # 本スクリプトのあるディレクトリ
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"                    # プロジェクトのルートディレクトリ
BUILD_DIR="${1:-Debug}"                                      # ビルドディレクトリ (第1引数、省略時はDebug)

# --- ldb関連 -----------------------------------------------------------------
# 環境変数が未設定でも、set -uで停止しないよう、${VAR:-}の形式で参照する
LDB_BIN="${LDB_BIN:-}"                                       # ldbのパス (未設定の場合は空文字列)
LDB_AVAILABLE=0                                              # ldbスキャンの実行可否 (0: スキップ、1: 実行)

# 以降の相対パス (ビルドディレクトリなど) を、プロジェクトルート基準にする
cd "$PROJECT_ROOT"

# -----------------------------------------------------------------------------
# 0. ldbの探索(任意)
# -----------------------------------------------------------------------------
# 探索順序は、LDB_BIN、ROCKSDB_ROOT/tools/ldb、PATH上のldbとする
# ROCKSDB_ROOTも未設定を許容するため、${ROCKSDB_ROOT:-}の形式で参照する
if [[ -z "$LDB_BIN" ]]; then
    if [[ -n "${ROCKSDB_ROOT:-}" && -x "${ROCKSDB_ROOT}/tools/ldb" ]]; then
        LDB_BIN="${ROCKSDB_ROOT}/tools/ldb"
    else
        # ldbが見つからなくても、set -eで終了しないよう、|| trueで失敗を無視する
        # 見つからない場合のエラーメッセージは不要なため、標準エラー出力を捨てる
        LDB_BIN="$(command -v ldb 2>/dev/null || true)"
    fi
fi

# ldbが見つからなくても異常終了とはせず、後続のldbスキャンだけをスキップする
if [[ -z "$LDB_BIN" ]]; then
    echo "INFO: ldb not found (set ROCKSDB_ROOT or LDB_BIN to enable ldb scan)."
    echo "      QTest step is still authoritative and will run."
    LDB_AVAILABLE=0
else
    echo "INFO: Using ldb: $LDB_BIN"
    LDB_AVAILABLE=1
fi

# -----------------------------------------------------------------------------
# 1. テスト有効でCMakeを構成する (再実行しても問題ない)
# -----------------------------------------------------------------------------
mkdir -p "$BUILD_DIR"
echo ">>> Configuring CMake (BUILD_TESTING=ON) into $BUILD_DIR ..."
# 構成時の標準出力は不要なため捨てる (エラーは標準エラー出力に残る)
# サブシェルで実行し、カレントディレクトリの変更を本体に影響させない
if ! (cd "$BUILD_DIR" && cmake .. -DBUILD_TESTING=ON >/dev/null); then
    # エラーメッセージは標準エラー出力へ出力する
    echo "ERROR: CMake configuration failed." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# 2. create_sample_dbとtest_rocksdbbackendをビルドする
# -----------------------------------------------------------------------------
echo ">>> Building create_sample_db and test_rocksdbbackend ..."
# [-j]にCPUコア数(nproc)を渡して、並列にビルドする
if ! cmake --build "$BUILD_DIR" --target create_sample_db test_rocksdbbackend -j"$(nproc)"; then
    # エラーメッセージは標準エラー出力へ出力する
    echo "ERROR: Build failed." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# 3. QTestの実行 (自動検証の主体)
# -----------------------------------------------------------------------------
# サブシェルで実行し、カレントディレクトリの変更を本体に影響させない
# 失敗した場合は、set -eにより、スクリプト全体がQTestの終了コードで終了する
echo ">>> Running QTest test_rocksdbbackend ..."
( cd "$BUILD_DIR" && ./tests/test_rocksdbbackend )
echo ">>> QTest passed."

# -----------------------------------------------------------------------------
# 4. 編集後DBのldbスキャン (任意)
# -----------------------------------------------------------------------------
# ldbが無い場合は、ここで正常終了する
if [[ "$LDB_AVAILABLE" -eq 0 ]]; then
    echo ">>> Skipping ldb scan (no ldb binary). Done."
    exit 0
fi

WORK="$(mktemp -d)"             # 一時作業ディレクトリ
trap 'rm -rf "$WORK"' EXIT      # 終了時 (異常終了を含む) に、一時ディレクトリを削除する
SAMPLE_DB="$WORK/sample-db"     # スキャン対象のサンプルDB

echo ">>> Generating sample DB at $SAMPLE_DB ..."
# 生成ツールの標準出力は不要なため捨てる
"$BUILD_DIR/create_sample_db" "$SAMPLE_DB" >/dev/null

# 編集処理は、QTest側で検証済みである
# testFormatVersionPreservedIssue2Regressionが、format_version=2のDBに対して、open、setData、closeの一連の処理を実行している
# 本ステップは、独立した確認として、生成直後のDBをldbでスキャンする
echo ">>> ldb scan on the generated DB ..."
# 標準出力は捨て、標準エラー出力はファイルに退避して、失敗時のみ表示する
# [--ignore_unknown_options]は、古いldbが、新しいOPTIONSの項目で失敗しないようにするために付ける
if "$LDB_BIN" --db="$SAMPLE_DB" --ignore_unknown_options scan >/dev/null 2>"$WORK/ldb.err"; then
    echo ">>> ldb scan: OK (no corruption, options accepted)."
else
    # エラーメッセージと退避しておいたldbのエラー出力は、標準エラー出力へ出力する
    echo "ERROR: ldb scan failed." >&2
    cat "$WORK/ldb.err" >&2
    exit 1
fi

echo ">>> All Issue #2 checks passed."
