#!/usr/bin/env python3
"""
RocksDB GUI Viewer (Desktop / pywebview版)

pywebview を利用してネイティブウィンドウ内に Web UI を表示し、
HTTP サーバを介さずに rocksdict 経由で RocksDB を直接操作します。

データベースは 1 ディレクトリで完結し、Column Family は RocksDB ネイティブの
機能を利用します。起動時に DB パスを与えなくても、ウィンドウ内の
「データベースを開く」ボタンから後から選択できます。

必要なパッケージ:
    pip install pywebview rocksdict

使用方法:
    python RocksDBViewer.py [データベースパス]

    引数を省略するとウィンドウ起動後にフォルダ選択ダイアログから DB を指定できます。
"""

from __future__ import annotations

import atexit
import json
import os
import sys
from pathlib import Path
from typing import Any

import webview
from webview import FileDialog
from rocksdict import Options, Rdict


# ----- ウィンドウ状態の永続化ヘルパ ------------------------------------

def _config_dir() -> Path:
    """ウィンドウ状態などを保存するアプリ設定ディレクトリ。

    Linux: $XDG_CONFIG_HOME/RocksDBViewer  (既定 ~/.config/RocksDBViewer)
    macOS: ~/Library/Application Support/RocksDBViewer
    Windows: %APPDATA%/RocksDBViewer
    """
    if sys.platform == 'win32':
        base = os.environ.get('APPDATA', os.path.expanduser('~'))
    elif sys.platform == 'darwin':
        base = os.path.expanduser('~/Library/Application Support')
    else:
        base = os.environ.get('XDG_CONFIG_HOME') or os.path.expanduser('~/.config')
    return Path(base) / 'RocksDBViewer'


_WINDOW_STATE_FILE = 'window.json'
DEFAULT_WIDTH = 1400
DEFAULT_HEIGHT = 900


def load_window_state() -> dict[str, Any]:
    """前回のウィンドウサイズ/位置を読み込む。読めなければ空 dict。"""
    try:
        path = _config_dir() / _WINDOW_STATE_FILE
        if path.exists():
            return json.loads(path.read_text(encoding='utf-8'))
    except Exception:
        pass
    return {}


def save_window_state(window: webview.Window) -> None:
    """終了時にウィンドウサイズ/位置を保存する (失敗しても無視)。"""
    try:
        cfg = _config_dir()
        cfg.mkdir(parents=True, exist_ok=True)
        state = {
            'width': int(window.width),
            'height': int(window.height),
            'x': int(window.x) if window.x is not None else None,
            'y': int(window.y) if window.y is not None else None,
        }
        (cfg / _WINDOW_STATE_FILE).write_text(
            json.dumps(state, indent=2), encoding='utf-8'
        )
    except Exception as e:
        print(f'Warning: failed to save window state: {e}', file=sys.stderr)


class RocksDBApi:
    """pywebview の js_api として公開する RocksDB 操作クラス。

    DB は遅延オープン可能。`open_database(path)` 呼び出しまでは未接続状態を
    維持し、すべての CRUD メソッドは未接続なら 'no_database' を返します。
    """

    def __init__(self, db_path: str | None = None) -> None:
        self.db_path: str | None = None
        self._db: Rdict | None = None
        self._cf_names: list[str] = []
        self._window: webview.Window | None = None
        atexit.register(self.close)

        if db_path and os.path.exists(db_path):
            self._open(db_path)

    # ---------- internal helpers ----------

    def attach_window(self, window: webview.Window) -> None:
        self._window = window

    def _open(self, db_path: str) -> None:
        """指定パスの RocksDB を開き、すべての既存 CF をハンドル可能にする。"""
        self._close_db()
        if os.path.exists(db_path):
            try:
                cf_names = Rdict.list_cf(db_path)
            except Exception:
                cf_names = ['default']
        else:
            cf_names = ['default']
        self._db = Rdict(
            db_path,
            column_families={name: Options() for name in cf_names},
        )
        self._cf_names = list(cf_names)
        self.db_path = db_path

    def _close_db(self) -> None:
        if self._db is not None:
            try:
                self._db.close()
            except Exception:
                pass
        self._db = None
        self._cf_names = []
        self.db_path = None

    def close(self) -> None:
        self._close_db()

    def _cf(self, column_family: str) -> Rdict:
        assert self._db is not None
        if column_family == 'default':
            return self._db
        return self._db.get_column_family(column_family)

    def _require_db(self) -> dict[str, Any] | None:
        if self._db is None:
            return {'status': 'no_database', 'message': 'No database is open'}
        return None

    # ---------- DB lifecycle (JS から呼ばれる) ----------

    def health(self) -> dict[str, Any]:
        if self._db is None:
            return {'status': 'no_database', 'database_path': None, 'database_exists': False}
        return {
            'status': 'ok',
            'database_path': os.path.abspath(self.db_path or ''),
            'database_exists': os.path.exists(self.db_path or ''),
        }

    def pick_directory(self) -> dict[str, Any]:
        """ネイティブのフォルダ選択ダイアログを開いて選択パスを返す。"""
        if self._window is None:
            return {'status': 'error', 'message': 'Window not ready'}
        try:
            initial = os.path.dirname(self.db_path) if self.db_path else os.getcwd()
            result = self._window.create_file_dialog(
                FileDialog.FOLDER,
                directory=initial,
            )
        except Exception as e:
            return {'status': 'error', 'message': str(e)}
        if not result:
            return {'status': 'cancelled', 'path': None}
        path = result[0] if isinstance(result, (list, tuple)) else result
        return {'status': 'success', 'path': path}

    def open_database(self, path: str) -> dict[str, Any]:
        """指定パスの RocksDB を開き、CF 一覧を返す。"""
        if not path:
            return {'status': 'error', 'message': 'path is required'}
        try:
            abs_path = os.path.abspath(os.path.expanduser(path)).rstrip('/')
            if not os.path.exists(abs_path):
                return {'status': 'error', 'message': f'Path does not exist: {abs_path}'}
            self._open(abs_path)
            return {
                'status': 'success',
                'database_path': abs_path,
                'column_families': self._cf_names,
            }
        except Exception as e:
            return {'status': 'error', 'message': str(e)}

    # ---------- CRUD (JS から呼ばれる) ----------

    def get_column_families(self) -> dict[str, Any]:
        err = self._require_db()
        if err:
            return err
        return {'status': 'success', 'column_families': self._cf_names}

    def get_data(self, column_family: str, search: str | None = None) -> dict[str, Any]:
        err = self._require_db()
        if err:
            return err
        try:
            cf = self._cf(column_family)
            term = (search or '').lower()
            data: dict[str, Any] = {}
            for key in cf.keys():
                value = cf[key]
                if term:
                    if term in str(key).lower() or term in str(value).lower():
                        data[key] = value
                else:
                    data[key] = value
            return {
                'status': 'success',
                'column_family': column_family,
                'data': data,
                'count': len(data),
            }
        except Exception as e:
            return {'status': 'error', 'message': str(e)}

    def get_data_by_key(self, column_family: str, key: str) -> dict[str, Any]:
        err = self._require_db()
        if err:
            return err
        try:
            cf = self._cf(column_family)
            if key in cf:
                return {'status': 'success', 'key': key, 'value': cf[key]}
            return {'status': 'error', 'message': f"Key '{key}' not found"}
        except Exception as e:
            return {'status': 'error', 'message': str(e)}

    def set_data(self, column_family: str, key: str, value: Any) -> dict[str, Any]:
        err = self._require_db()
        if err:
            return err
        if not key:
            return {'status': 'error', 'message': 'key and value are required'}
        try:
            self._cf(column_family)[key] = value
            return {'status': 'success', 'message': 'Data saved successfully', 'key': key}
        except Exception as e:
            return {'status': 'error', 'message': str(e)}

    def delete_data(self, column_family: str, key: str) -> dict[str, Any]:
        err = self._require_db()
        if err:
            return err
        try:
            cf = self._cf(column_family)
            if key in cf:
                del cf[key]
                return {'status': 'success', 'message': f"Key '{key}' deleted successfully"}
            return {'status': 'error', 'message': f"Key '{key}' not found"}
        except Exception as e:
            return {'status': 'error', 'message': str(e)}

    def clear_data(self, column_family: str) -> dict[str, Any]:
        err = self._require_db()
        if err:
            return err
        try:
            cf = self._cf(column_family)
            keys = list(cf.keys())
            for key in keys:
                del cf[key]
            return {
                'status': 'success',
                'message': f"All data in '{column_family}' deleted successfully",
                'deleted_count': len(keys),
            }
        except Exception as e:
            return {'status': 'error', 'message': str(e)}

    def export_data(self, column_family: str) -> dict[str, Any]:
        err = self._require_db()
        if err:
            return err
        try:
            cf = self._cf(column_family)
            data: dict[str, Any] = {}
            for key in cf.keys():
                value = cf[key]
                try:
                    data[key] = json.loads(value)
                except (json.JSONDecodeError, TypeError):
                    data[key] = value
            return {'status': 'success', 'column_family': column_family, 'data': data}
        except Exception as e:
            return {'status': 'error', 'message': str(e)}

    def import_data(self, column_family: str, data: Any) -> dict[str, Any]:
        err = self._require_db()
        if err:
            return err
        if not isinstance(data, dict):
            return {'status': 'error', 'message': 'Invalid data format. Expected JSON object.'}
        try:
            cf = self._cf(column_family)
            count = 0
            for key, value in data.items():
                if isinstance(value, (dict, list)):
                    cf[key] = json.dumps(value, ensure_ascii=False)
                else:
                    cf[key] = str(value)
                count += 1
            return {
                'status': 'success',
                'message': f'{count} records imported successfully',
                'imported_count': count,
            }
        except Exception as e:
            return {'status': 'error', 'message': str(e)}


def resolve_db_path(arg_path: str | None) -> str | None:
    if not arg_path:
        return None
    path = os.path.abspath(os.path.expanduser(arg_path))
    return path.rstrip('/')


def print_banner(db_path: str | None) -> None:
    print('=' * 60)
    print('RocksDB GUI Viewer (Desktop / pywebview)')
    print('=' * 60)
    print()
    if db_path:
        print(f'データベースパス: {db_path}')
    else:
        print('データベースパスは未指定です。ウィンドウから選択してください。')
    print('ネイティブウィンドウを起動しています...')
    print()


def main() -> int:
    db_path = resolve_db_path(sys.argv[1] if len(sys.argv) > 1 else None)

    if db_path and not os.path.exists(db_path):
        print(f'警告: データベースが見つかりません: {db_path}')
        print('ウィンドウからフォルダを選び直してください。')
        print()
        db_path = None

    print_banner(db_path)

    api = RocksDBApi(db_path)
    base_dir = os.path.dirname(os.path.abspath(__file__))
    html_path = os.path.join(base_dir, 'RocksDBViewer.html')
    icon_path = os.path.join(base_dir, 'assets', 'RocksDBViewer.png')

    state = load_window_state()
    window = webview.create_window(
        title='RocksDB GUI Viewer',
        url=html_path,
        js_api=api,
        width=int(state.get('width') or DEFAULT_WIDTH),
        height=int(state.get('height') or DEFAULT_HEIGHT),
        x=state.get('x'),
        y=state.get('y'),
        resizable=True,
        text_select=True,
    )
    api.attach_window(window)

    # 終了時にウィンドウサイズ/位置を保存
    window.events.closing += lambda: save_window_state(window)

    webview.start(icon=icon_path if os.path.exists(icon_path) else None)
    return 0


if __name__ == '__main__':
    sys.exit(main())
