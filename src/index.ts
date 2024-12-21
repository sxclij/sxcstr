import {
    generateSecretKey,
    getPublicKey,
    finalizeEvent,
    verifyEvent
} from 'nostr-tools/pure';
import { SimplePool } from 'nostr-tools/pool';
import { nip19 } from 'nostr-tools';
import { useWebSocketImplementation } from 'nostr-tools/relay';
import WebSocket from 'ws';


useWebSocketImplementation(WebSocket);


const privateKey = generateSecretKey();
const publicKey = getPublicKey(privateKey);

console.log("秘密鍵 (Hex):", privateKey);
console.log("公開鍵 (Hex):", publicKey);

const npub = nip19.npubEncode(publicKey);
console.log("公開鍵 (Npub):", npub);

const pool = new SimplePool();
const relays = ['wss://relay.damus.io', 'wss://relay.snort.social', 'wss://nostr.wine'];

// メッセージ表示領域の要素を取得
const messageContainer = document.getElementById('messages') as HTMLDivElement;


// イベントを受信した際の処理
const sub = pool.subscribeMany(
    relays,
    [
        {
            kinds: [1],
            authors: [publicKey],
        },
    ],
    {
        onevent(event) {
            console.log('受信したイベント:', event);
            const messageElement = document.createElement('p');
            messageElement.textContent = `[${new Date(event.created_at * 1000).toLocaleTimeString()}] ${event.content}`;
            messageContainer.appendChild(messageElement);
        },
        oneose() {
            console.log('購読終了');
            sub.close();
        }
    }
)
// イベントを作成して送信
const eventTemplate = {
    kind: 1,
    created_at: Math.floor(Date.now() / 1000),
    tags: [],
    content: 'こんにちは、Nostr！'
};

const signedEvent = finalizeEvent(eventTemplate, privateKey);
pool.publish(relays, signedEvent).then(() => {
    console.log('publish')
});