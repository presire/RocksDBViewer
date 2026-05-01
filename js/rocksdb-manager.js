// RocksDB GUI Manager - Main JavaScript (Desktop / pywebview版)
// Python (rocksdict) 直接呼び出しでRocksDBを操作
//
// すべてのDB操作は window.pywebview.api.<method>(...) 経由で行われ、
// REST API・HTTP・CORSは介在しません。

// pywebview API ヘルパ
// pywebview の準備が整う前に呼ばれた場合は pywebviewready を待ちます
function pyApi() {
    if (window.pywebview && window.pywebview.api) {
        return Promise.resolve(window.pywebview.api);
    }
    return new Promise((resolve) => {
        window.addEventListener('pywebviewready', () => {
            resolve(window.pywebview.api);
        }, { once: true });
    });
}

// ============================================================================
// 言語定義
// ============================================================================
const translations = {
    ja: {
        // ヘッダー
        subtitle: 'rocksdictにより直接アクセス (デスクトップ版)',
        language: '言語',
        theme: 'テーマ',
        serverStatus: 'ステータス',
        notConnected: '未接続',
        connected: '接続済み',
        connectionFailed: '接続失敗',

        // データベースパネル
        databasePath: 'データベースパス',
        openDatabase: 'データベースを開く',
        noDatabaseSelected: 'データベースが選択されていません',
        welcomeMessage: '「データベースを開く」ボタンから RocksDB ディレクトリを選択してください。',

        // メインコンテンツ
        columnFamily: 'カラムファミリー',
        selectColumnFamily: 'カラムファミリーを選択',
        search: '検索',
        searchPlaceholder: 'キーまたはバリューで検索...',
        entryCount: 'エントリー数',
        entries: '件',

        // アクションボタン
        addNew: '新規追加',
        refresh: '更新',
        autoRefreshOff: '自動更新OFF',
        autoRefreshOn: '自動更新ON',
        sort: 'ソート',
        export: 'エクスポート',
        import: 'インポート',
        clearAll: '全削除',

        // テーブル
        key: 'キー',
        value: 'バリュー',
        actions: '操作',
        edit: '編集',
        delete: '削除',

        // モーダル
        editEntry: 'エントリーを編集',
        addEntry: 'エントリーを追加',
        keyPlaceholder: '例: user:123',
        valuePlaceholder: '例: {"name": "太郎", "age": 30}',
        format: 'フォーマット',
        minify: '圧縮',
        save: '保存',
        cancel: 'キャンセル',

        // 状態
        loading: 'データを読み込んでいます...',
        noData: 'データがありません',
        noDataDesc: '新しいエントリーを追加してください',
        addFirstEntry: '最初のエントリーを追加',

        // トーストメッセージ
        toast: {
            serverConnected: 'サーバに接続しました',
            serverConnectionFailed: 'サーバへの接続に失敗しました',
            autoRefreshStarted: '自動更新を開始しました（10秒間隔）',
            autoRefreshStopped: '自動更新を停止しました',
            dataRefreshed: 'データを再読み込みしました',
            jsonFormatted: 'JSONをフォーマットしました',
            jsonMinified: 'JSONを圧縮しました',
            invalidJson: '有効なJSON形式ではありません',
            dataDeleted: 'データを削除しました',
            deleteFailed: '削除に失敗しました',
            allDataDeleted: '全データを削除しました',
            dataSaved: 'データを保存しました',
            saveFailed: '保存に失敗しました',
            enterKeyValue: 'キーとバリューの両方を入力してください',
            exportSuccess: 'データをエクスポートしました',
            importSuccess: 'データをインポートしました',
            importFailed: 'インポートに失敗しました'
        },

        // 確認ダイアログ
        confirm: {
            deleteKey: 'キー "%key%" を削除してもよろしいですか?\nこの操作は取り消せません。',
            clearAll: 'カラムファミリー "%cf%" の全データを削除してもよろしいですか?\nこの操作は取り消せません。すべてのデータが完全に削除されます。'
        },

        // JSON検証
        validJson: '有効なJSON形式です',
        notJsonWarning: 'JSON形式ではありません（文字列として保存されます）'
    },

    en: {
        // Header
        subtitle: 'Direct access via rocksdict (Desktop Edition)',
        language: 'Language',
        theme: 'Theme',
        serverStatus: 'Status',
        notConnected: 'Not Connected',
        connected: 'Connected',
        connectionFailed: 'Connection Failed',

        // Database Panel
        databasePath: 'Database Path',
        openDatabase: 'Open Database',
        noDatabaseSelected: 'No database selected',
        welcomeMessage: 'Click "Open Database" to select a RocksDB directory.',

        // Main Content
        columnFamily: 'Column Family',
        selectColumnFamily: 'Select Column Family',
        search: 'Search',
        searchPlaceholder: 'Search by key or value...',
        entryCount: 'Entry Count',
        entries: 'entries',

        // Action Buttons
        addNew: 'Add New',
        refresh: 'Refresh',
        autoRefreshOff: 'Auto-Refresh OFF',
        autoRefreshOn: 'Auto-Refresh ON',
        sort: 'Sort',
        export: 'Export',
        import: 'Import',
        clearAll: 'Clear All',

        // Table
        key: 'Key',
        value: 'Value',
        actions: 'Actions',
        edit: 'Edit',
        delete: 'Delete',

        // Modal
        editEntry: 'Edit Entry',
        addEntry: 'Add Entry',
        keyPlaceholder: 'e.g., user:123',
        valuePlaceholder: 'e.g., {"name": "John", "age": 30}',
        format: 'Format',
        minify: 'Minify',
        save: 'Save',
        cancel: 'Cancel',

        // States
        loading: 'Loading data...',
        noData: 'No Data',
        noDataDesc: 'Please add a new entry',
        addFirstEntry: 'Add First Entry',

        // Toast Messages
        toast: {
            serverConnected: 'Connected to server',
            serverConnectionFailed: 'Failed to connect to server',
            autoRefreshStarted: 'Auto-refresh started (10 second interval)',
            autoRefreshStopped: 'Auto-refresh stopped',
            dataRefreshed: 'Data refreshed',
            jsonFormatted: 'JSON formatted',
            jsonMinified: 'JSON minified',
            invalidJson: 'Invalid JSON format',
            dataDeleted: 'Data deleted',
            deleteFailed: 'Failed to delete',
            allDataDeleted: 'All data deleted',
            dataSaved: 'Data saved',
            saveFailed: 'Failed to save',
            enterKeyValue: 'Please enter both key and value',
            exportSuccess: 'Data exported',
            importSuccess: 'Data imported',
            importFailed: 'Failed to import'
        },

        // Confirm Dialogs
        confirm: {
            deleteKey: 'Are you sure you want to delete key "%key%"?\nThis operation cannot be undone.',
            clearAll: 'Are you sure you want to delete all data in column family "%cf%"?\nThis operation cannot be undone. All data will be permanently deleted.'
        },

        // JSON Validation
        validJson: 'Valid JSON format',
        notJsonWarning: 'Not in JSON format (will be saved as string)'
    }
};

// 現在の言語
let currentLanguage = 'ja';

// ============================================================================
// 言語切り替え関数
// ============================================================================

/**
 * 言語を切り替える
 */
function toggleLanguage() {
    const toggle = document.getElementById('languageToggle');
    currentLanguage = toggle.checked ? 'en' : 'ja';

    // ローカルストレージに保存
    localStorage.setItem('rocksdb_language', currentLanguage);

    // UI要素を更新
    updateLanguage();
}

/**
 * 翻訳テキストを取得
 */
function t(key) {
    const keys = key.split('.');
    let value = translations[currentLanguage];

    for (const k of keys) {
        if (value && typeof value === 'object') {
            value = value[k];
        } else {
            return key;
        }
    }

    return value || key;
}

/**
 * 全UI要素の言語を更新
 */
function updateLanguage() {
    // data-i18n属性を持つ要素を更新
    document.querySelectorAll('[data-i18n]').forEach(element => {
        const key = element.getAttribute('data-i18n');
        element.textContent = t(key);
    });

    // data-i18n-placeholder属性を持つ要素を更新
    document.querySelectorAll('[data-i18n-placeholder]').forEach(element => {
        const key = element.getAttribute('data-i18n-placeholder');
        element.placeholder = t(key);
    });

    // 動的コンテンツを更新
    updateDynamicContent();
}

/**
 * 動的コンテンツの言語を更新
 */
function updateDynamicContent() {
    // ステータス表示を更新
    if (isConnected) {
        dbStatus.innerHTML = `
            <span class="w-2 h-2 mr-2 rounded-full bg-green-500 animate-pulse"></span>
            <span data-i18n="connected">${t('connected')}</span>
        `;
    } else {
        dbStatus.innerHTML = `
            <span class="w-2 h-2 mr-2 rounded-full bg-gray-400"></span>
            <span data-i18n="notConnected">${t('notConnected')}</span>
        `;
    }

    // 自動更新ボタンを更新
    updateAutoRefreshUI(autoRefreshEnabled);
}

/**
 * 保存された言語設定を読み込む
 */
function loadLanguagePreference() {
    const savedLanguage = localStorage.getItem('rocksdb_language');
    if (savedLanguage && translations[savedLanguage]) {
        currentLanguage = savedLanguage;
        document.getElementById('languageToggle').checked = (currentLanguage === 'en');
        updateLanguage();
    }
}

// ============================================================================
// テーマ切替 (ライト / ダーク)
// ============================================================================

/**
 * 現在のテーマを切り替え、localStorage に保存する。
 */
function toggleTheme() {
    const current = document.documentElement.getAttribute('data-theme') || 'light';
    const next = current === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', next);
    localStorage.setItem('rocksdb_theme', next);
}

/**
 * 起動時にテーマ設定を復元。OS のダーク設定もフォールバックとして使う。
 */
function loadThemePreference() {
    const saved = localStorage.getItem('rocksdb_theme');
    let theme = saved;
    if (!theme) {
        theme = (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches)
            ? 'dark'
            : 'light';
    }
    document.documentElement.setAttribute('data-theme', theme);
}

// ============================================================================
// グローバル変数の宣言
// ============================================================================
let databasePath = ''; // データベースパスを保存
let currentColumnFamily = '';
let isConnected = false;
let autoRefreshInterval = null;
let autoRefreshEnabled = false;
let sortOrder = 'none'; // 'none', 'asc', 'desc'

// DOM要素の取得
const dbPathDisplay = document.getElementById('dbPathDisplay');
const dbStatus = document.getElementById('dbStatus');
const mainContent = document.getElementById('mainContent');
const welcomeState = document.getElementById('welcomeState');
const openDbBtn = document.getElementById('openDbBtn');
const columnFamilySelect = document.getElementById('columnFamilySelect');
const searchInput = document.getElementById('searchInput');
const entryCount = document.getElementById('entryCount');
const addEntryBtn = document.getElementById('addEntryBtn');
const refreshBtn = document.getElementById('refreshBtn');
const clearAllBtn = document.getElementById('clearAllBtn');
const importFileInput = document.getElementById('importFileInput');
const dataTableBody = document.getElementById('dataTableBody');
const loadingState = document.getElementById('loadingState');
const emptyState = document.getElementById('emptyState');
const editModal = document.getElementById('editModal');
const modalTitle = document.getElementById('modalTitle');
const editForm = document.getElementById('editForm');
const keyInput = document.getElementById('keyInput');
const valueInput = document.getElementById('valueInput');
const jsonValidation = document.getElementById('jsonValidation');
const formatJsonBtn = document.getElementById('formatJsonBtn');
const minifyJsonBtn = document.getElementById('minifyJsonBtn');
const toastContainer = document.getElementById('toastContainer');

// 自動更新設定を復元
function loadAutoRefreshPreference() {
    const savedAutoRefresh = localStorage.getItem('rocksdb_auto_refresh');
    if (savedAutoRefresh === 'true') {
        autoRefreshEnabled = true;
    }
}

// pywebview 経由でRocksDBに接続 (起動時に自動実行)
// 起動時にDBが既に開かれていればそれを反映、未選択なら welcome 画面を表示
async function connectToDatabase() {
    try {
        const api = await pyApi();
        const data = await api.health();

        if (data.status === 'ok') {
            applyConnectedState(data.database_path);
            await loadColumnFamilies();
            if (autoRefreshEnabled) {
                startAutoRefresh();
            }
        } else {
            // 'no_database' などの未接続状態
            applyDisconnectedState();
        }
    } catch (error) {
        console.error('接続エラー:', error);
        showToast(t('toast.serverConnectionFailed') + ': ' + error.message, 'error');
        applyDisconnectedState();
    }
}

// DB を開いている状態の UI 更新
function applyConnectedState(path) {
    isConnected = true;
    databasePath = path || 'Unknown';

    if (dbPathDisplay) {
        dbPathDisplay.textContent = databasePath;
        dbPathDisplay.title = databasePath;
    }

    updateConnectionUI(true);
    mainContent.classList.remove('hidden');
    if (welcomeState) {
        welcomeState.classList.add('hidden');
    }
}

// DB が未選択の状態の UI 更新
function applyDisconnectedState() {
    isConnected = false;
    databasePath = '';
    currentColumnFamily = '';

    if (dbPathDisplay) {
        dbPathDisplay.textContent = t('noDatabaseSelected');
        dbPathDisplay.title = '';
    }

    updateConnectionUI(false);
    mainContent.classList.add('hidden');
    if (welcomeState) {
        welcomeState.classList.remove('hidden');
    }
}

// 「データベースを開く」フロー: ネイティブダイアログ → DB を開いて UI を更新
async function openDatabaseFlow() {
    try {
        const api = await pyApi();
        const pick = await api.pick_directory();

        if (pick.status === 'cancelled') {
            return;
        }
        if (pick.status !== 'success' || !pick.path) {
            showToast(pick.message || 'Failed to open dialog', 'error');
            return;
        }

        const result = await api.open_database(pick.path);
        if (result.status !== 'success') {
            showToast(t('toast.serverConnectionFailed') + ': ' + (result.message || ''), 'error');
            return;
        }

        // 自動更新を一旦停止 (新DBに対して再開)
        stopAutoRefresh();

        applyConnectedState(result.database_path);
        sortOrder = 'none';
        await loadColumnFamilies();

        if (autoRefreshEnabled) {
            startAutoRefresh();
        }

        showToast(t('toast.serverConnected'), 'success');
    } catch (error) {
        console.error('Open database error:', error);
        showToast(t('toast.serverConnectionFailed') + ': ' + error.message, 'error');
    }
}

// 接続状態に応じたUI更新
function updateConnectionUI(connected) {
    if (connected) {
        dbStatus.innerHTML = `
            <span class="w-2 h-2 mr-2 rounded-full bg-green-500 animate-pulse"></span>
            <span data-i18n="connected">${t('connected')}</span>
        `;
        dbStatus.className = 'inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-green-100 text-green-700';
    } else {
        dbStatus.innerHTML = `
            <span class="w-2 h-2 mr-2 rounded-full bg-gray-400"></span>
            <span data-i18n="notConnected">${t('notConnected')}</span>
        `;
        dbStatus.className = 'inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-gray-100 text-gray-600';
    }
}

// 自動更新を開始
function startAutoRefresh(intervalSeconds = 10) {
    if (autoRefreshInterval) {
        clearInterval(autoRefreshInterval);
    }

    autoRefreshInterval = setInterval(() => {
        if (isConnected && currentColumnFamily) {
            updateTable(true); // サイレントモード（トーストを表示しない）
        }
    }, intervalSeconds * 1000);

    autoRefreshEnabled = true;
    localStorage.setItem('rocksdb_auto_refresh', 'true');
    updateAutoRefreshUI(true);
}

// 自動更新を停止
function stopAutoRefresh() {
    if (autoRefreshInterval) {
        clearInterval(autoRefreshInterval);
        autoRefreshInterval = null;
    }

    autoRefreshEnabled = false;
    localStorage.setItem('rocksdb_auto_refresh', 'false');
    updateAutoRefreshUI(false);
}

// 自動更新トグル
function toggleAutoRefresh() {
    if (autoRefreshEnabled) {
        stopAutoRefresh();
        showToast(t('toast.autoRefreshStopped'), 'info');
    } else {
        startAutoRefresh(10);
        showToast(t('toast.autoRefreshStarted'), 'success');
    }
}

// 自動更新UIを更新
function updateAutoRefreshUI(enabled) {
    const autoRefreshBtn = document.getElementById('autoRefreshBtn');
    if (!autoRefreshBtn) return;

    if (enabled) {
        autoRefreshBtn.innerHTML = `
            <span class="flex items-center gap-2">
                <svg class="w-5 h-5 animate-spin" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"></path>
                </svg>
                <span data-i18n="autoRefreshOn">${t('autoRefreshOn')}</span>
            </span>
        `;
        autoRefreshBtn.className = 'px-5 py-2.5 bg-green-600 text-white font-semibold rounded-lg hover:bg-green-700 focus:outline-none focus:ring-2 focus:ring-green-500 transition-all shadow-md';
    } else {
        autoRefreshBtn.innerHTML = `
            <span class="flex items-center gap-2">
                <svg class="w-5 h-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"></path>
                </svg>
                <span data-i18n="autoRefreshOff">${t('autoRefreshOff')}</span>
            </span>
        `;
        autoRefreshBtn.className = 'px-5 py-2.5 bg-gray-600 text-white font-semibold rounded-lg hover:bg-gray-700 focus:outline-none focus:ring-2 focus:ring-gray-500 transition-all shadow-md hover:shadow-lg transform hover:scale-105';
    }
}

// カラムファミリーの一覧を取得
async function loadColumnFamilies() {
    try {
        const api = await pyApi();
        const data = await api.get_column_families();

        if (data.status !== 'success') {
            throw new Error(data.message);
        }

        // セレクトボックスをクリアして再構築
        columnFamilySelect.innerHTML = `<option value="">${t('selectColumnFamily')}</option>`;

        data.column_families.forEach(cf => {
            const option = document.createElement('option');
            option.value = cf;
            option.textContent = cf;
            columnFamilySelect.appendChild(option);
        });

        // デフォルトのカラムファミリーを選択
        if (data.column_families.length > 0) {
            currentColumnFamily = data.column_families[0];
            columnFamilySelect.value = currentColumnFamily;
            await updateTable();
        }
    } catch (error) {
        console.error('カラムファミリー取得エラー:', error);
        showToast('Failed to load column families: ' + error.message, 'error');
    }
}

// データテーブルを更新
async function updateTable(silent = false) {
    if (!currentColumnFamily) return;

    try {
        showLoading(true);

        const searchTerm = searchInput.value.trim();
        const api = await pyApi();
        const result = await api.get_data(currentColumnFamily, searchTerm || null);

        if (result.status !== 'success') {
            throw new Error(result.message);
        }

        // データをソート
        let entries = Object.entries(result.data);
        if (sortOrder === 'asc') {
            entries.sort((a, b) => a[0].localeCompare(b[0]));
        } else if (sortOrder === 'desc') {
            entries.sort((a, b) => b[0].localeCompare(a[0]));
        }

        // テーブルを更新
        renderTable(entries);

        // エントリー数を更新
        entryCount.textContent = entries.length.toString();

        showLoading(false);

        // 空の状態を表示/非表示
        if (entries.length === 0) {
            emptyState.classList.remove('hidden');
        } else {
            emptyState.classList.add('hidden');
        }

    } catch (error) {
        console.error('データ取得エラー:', error);
        if (!silent) {
            showToast('Failed to load data: ' + error.message, 'error');
        }
        showLoading(false);
    }
}

// テーブルを描画
function renderTable(entries) {
    dataTableBody.innerHTML = '';

    entries.forEach(([key, value]) => {
        const row = document.createElement('tr');
        row.className = 'table-row-hover';

        // 値の表示を整形
        let displayValue = value;
        try {
            const parsed = JSON.parse(value);
            displayValue = JSON.stringify(parsed, null, 2);
        } catch (e) {
            // JSONでない場合はそのまま表示
        }

        // 長い値は省略
        const maxLength = 200;
        if (displayValue.length > maxLength) {
            displayValue = displayValue.substring(0, maxLength) + '...';
        }

        row.innerHTML = `
            <td class="px-6 py-4">
                <code class="text-sm font-mono text-blue-600 break-all">${escapeHtml(key)}</code>
            </td>
            <td class="px-6 py-4">
                <pre class="text-sm text-gray-700 whitespace-pre-wrap break-words">${escapeHtml(displayValue)}</pre>
            </td>
            <td class="px-6 py-4 text-right">
                <div class="flex justify-end gap-2">
                    <button onclick="editEntry('${escapeHtml(key)}')"
                            title="${t('edit')}"
                            class="p-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-all transform hover:scale-110">
                        <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z"></path>
                        </svg>
                    </button>
                    <button onclick="deleteEntry('${escapeHtml(key)}')"
                            title="${t('delete')}"
                            class="p-2 bg-red-600 text-white rounded-lg hover:bg-red-700 transition-all transform hover:scale-110">
                        <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16"></path>
                        </svg>
                    </button>
                </div>
            </td>
        `;

        dataTableBody.appendChild(row);
    });
}

// HTMLエスケープ関数
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ソートをトグル
function toggleSort() {
    if (sortOrder === 'none') {
        sortOrder = 'asc';
    } else if (sortOrder === 'asc') {
        sortOrder = 'desc';
    } else {
        sortOrder = 'none';
    }

    updateTable();
}

// データをエクスポート
async function exportData() {
    try {
        const api = await pyApi();
        const result = await api.export_data(currentColumnFamily);

        if (result.status !== 'success') {
            throw new Error(result.message);
        }

        // JSONファイルとしてダウンロード
        const dataStr = JSON.stringify(result.data, null, 2);
        const dataBlob = new Blob([dataStr], { type: 'application/json' });
        const url = URL.createObjectURL(dataBlob);
        const link = document.createElement('a');
        link.href = url;
        link.download = `${currentColumnFamily}_${new Date().toISOString().split('T')[0]}.json`;
        link.click();
        URL.revokeObjectURL(url);

        showToast(t('toast.exportSuccess'), 'success');
    } catch (error) {
        console.error('エクスポートエラー:', error);
        showToast('Export failed: ' + error.message, 'error');
    }
}

// データをインポート
async function importData(event) {
    const file = event.target.files[0];
    if (!file) return;

    try {
        const text = await file.text();
        const data = JSON.parse(text);

        const api = await pyApi();
        const result = await api.import_data(currentColumnFamily, data);

        if (result.status !== 'success') {
            throw new Error(result.message);
        }

        await updateTable();
        showToast(t('toast.importSuccess'), 'success');
    } catch (error) {
        console.error('インポートエラー:', error);
        showToast(t('toast.importFailed') + ': ' + error.message, 'error');
    }

    // ファイル入力をリセット
    event.target.value = '';
}

// 編集モーダルを開く
function openEditModal(key = null, value = null) {
    editModal.classList.remove('hidden');

    if (key) {
        modalTitle.textContent = t('editEntry');
        keyInput.value = key;
        keyInput.disabled = true;
        valueInput.value = value || '';
    } else {
        modalTitle.textContent = t('addEntry');
        keyInput.value = '';
        keyInput.disabled = false;
        valueInput.value = '';
    }

    validateJSON(valueInput.value);
}

// 編集モーダルを閉じる
function closeEditModal() {
    editModal.classList.add('hidden');
    editForm.reset();
    jsonValidation.classList.add('hidden');
}

// エントリーを編集
async function editEntry(key) {
    try {
        const api = await pyApi();
        const result = await api.get_data_by_key(currentColumnFamily, key);

        if (result.status !== 'success') {
            throw new Error(result.message);
        }

        openEditModal(key, result.value);
    } catch (error) {
        console.error('データ取得エラー:', error);
        showToast('Failed to load data: ' + error.message, 'error');
    }
}

// JSON検証
function validateJSON(value) {
    try {
        if (!value.trim()) {
            jsonValidation.classList.add('hidden');
            return;
        }

        JSON.parse(value);
        jsonValidation.innerHTML = `
            <svg class="w-5 h-5 inline mr-2 text-green-600" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"></path>
            </svg>
            <span class="text-green-700 font-medium">${t('validJson')}</span>
        `;
        jsonValidation.className = 'mt-2 p-3 bg-green-50 border border-green-200 rounded-lg';
        jsonValidation.classList.remove('hidden');
    } catch (e) {
        // 空の場合は非表示
        if (!value.trim()) {
            jsonValidation.classList.add('hidden');
            return;
        }

        jsonValidation.innerHTML = `
            <svg class="w-5 h-5 inline mr-2 text-yellow-600" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path>
            </svg>
            <span class="text-yellow-700 font-medium">${t('notJsonWarning')}</span>
        `;
        jsonValidation.className = 'mt-2 p-3 bg-yellow-50 border border-yellow-200 rounded-lg';
        jsonValidation.classList.remove('hidden');
    }
}

// JSONをフォーマット
function formatJSON() {
    try {
        const value = valueInput.value.trim();
        if (!value) return;

        const parsed = JSON.parse(value);
        valueInput.value = JSON.stringify(parsed, null, 2);
        showToast(t('toast.jsonFormatted'), 'success');
    } catch (e) {
        showToast(t('toast.invalidJson'), 'error');
    }
}

// JSONを圧縮
function minifyJSON() {
    try {
        const value = valueInput.value.trim();
        if (!value) return;

        const parsed = JSON.parse(value);
        valueInput.value = JSON.stringify(parsed);
        showToast(t('toast.jsonMinified'), 'success');
    } catch (e) {
        showToast(t('toast.invalidJson'), 'error');
    }
}

// エントリーを削除
async function deleteEntry(key) {
    const message = t('confirm.deleteKey').replace('%key%', key);
    if (!confirm(message)) {
        return;
    }

    try {
        const api = await pyApi();
        const result = await api.delete_data(currentColumnFamily, key);

        if (result.status !== 'success') {
            throw new Error(result.message);
        }

        await updateTable();
        showToast(t('toast.dataDeleted'), 'success');
    } catch (error) {
        console.error('削除エラー:', error);
        showToast(t('toast.deleteFailed') + ': ' + error.message, 'error');
    }
}

// 全削除
async function clearAll() {
    const message = t('confirm.clearAll').replace('%cf%', currentColumnFamily);
    if (!confirm(message)) {
        return;
    }

    try {
        const api = await pyApi();
        const result = await api.clear_data(currentColumnFamily);

        if (result.status !== 'success') {
            throw new Error(result.message);
        }

        await updateTable();
        showToast(t('toast.allDataDeleted'), 'success');
    } catch (error) {
        console.error('削除エラー:', error);
        showToast(t('toast.deleteFailed') + ': ' + error.message, 'error');
    }
}

// トースト通知を表示
function showToast(message, type = 'info') {
    const toast = document.createElement('div');

    const colors = {
        success: 'bg-green-500',
        error: 'bg-red-500',
        info: 'bg-blue-500',
        warning: 'bg-yellow-500'
    };

    const icons = {
        success: `<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 13l4 4L19 7"></path>`,
        error: `<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path>`,
        info: `<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"></path>`,
        warning: `<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"></path>`
    };

    toast.className = `toast ${colors[type]} text-white rounded-lg shadow-2xl p-4 flex items-center gap-3`;
    toast.innerHTML = `
        <svg class="w-6 h-6 flex-shrink-0" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            ${icons[type]}
        </svg>
        <span class="flex-1 font-medium">${message}</span>
        <button onclick="this.parentElement.remove()" class="ml-2 hover:bg-white hover:bg-opacity-20 rounded p-1">
            <svg class="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12"></path>
            </svg>
        </button>
    `;

    toastContainer.appendChild(toast);

    setTimeout(() => {
        toast.classList.add('hide');
        setTimeout(() => toast.remove(), 300);
    }, 3000);
}

// ローディング状態を表示/非表示
function showLoading(show) {
    if (show) {
        loadingState.classList.remove('hidden');
        emptyState.classList.add('hidden');
    } else {
        loadingState.classList.add('hidden');
    }
}

// イベントリスナーの設定
function initializeEventListeners() {
    // 言語設定を読み込む
    loadLanguagePreference();

    // 自動更新設定を読み込む
    loadAutoRefreshPreference();

    // テーマ設定を読み込む
    loadThemePreference();

    // データベースを開くボタン
    if (openDbBtn) {
        openDbBtn.addEventListener('click', openDatabaseFlow);
    }

    // カラムファミリー変更
    columnFamilySelect.addEventListener('change', async (e) => {
        currentColumnFamily = e.target.value;
        searchInput.value = '';
        await updateTable();
    });

    // 検索
    searchInput.addEventListener('input', async () => {
        await updateTable();
    });

    // 各種ボタン
    addEntryBtn.addEventListener('click', () => openEditModal());
    refreshBtn.addEventListener('click', async () => {
        await updateTable();
        showToast(t('toast.dataRefreshed'), 'success');
    });

    clearAllBtn.addEventListener('click', clearAll);

    // JSON編集
    valueInput.addEventListener('input', (e) => {
        validateJSON(e.target.value);
    });

    formatJsonBtn.addEventListener('click', formatJSON);
    minifyJsonBtn.addEventListener('click', minifyJSON);

    // フォーム送信
    editForm.addEventListener('submit', async (e) => {
        e.preventDefault();

        const key = keyInput.value.trim();
        const value = valueInput.value.trim();

        if (!key || !value) {
            showToast(t('toast.enterKeyValue'), 'error');
            return;
        }

        try {
            const api = await pyApi();
            const result = await api.set_data(currentColumnFamily, key, value);

            if (result.status !== 'success') {
                throw new Error(result.message);
            }

            closeEditModal();
            await updateTable();
            showToast(t('toast.dataSaved'), 'success');
        } catch (error) {
            console.error('保存エラー:', error);
            showToast(t('toast.saveFailed') + ': ' + error.message, 'error');
        }
    });

    // モーダル外クリックで閉じる
    editModal.addEventListener('click', (e) => {
        if (e.target === editModal) {
            closeEditModal();
        }
    });
}

// 起動シーケンス: DOM準備 → イベントリスナー → pywebview API 経由でDB接続
document.addEventListener('DOMContentLoaded', () => {
    initializeEventListeners();
    // pywebview の API 準備を待ってから自動接続
    connectToDatabase();
});
