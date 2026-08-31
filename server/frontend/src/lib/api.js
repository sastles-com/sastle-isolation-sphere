// API 呼び出しの薄い共通ラッパー
// Response オブジェクトをそのまま返し、ok 判定やエラー処理は呼び出し側に委ねる
// (各コンポーネントの既存のエラーハンドリング挙動を維持するため)

export const apiGet = (path) => fetch(path);

export const apiPost = (path, body) =>
    fetch(path, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });

export const apiPut = (path, body) =>
    fetch(path, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });
