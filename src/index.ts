import {
    generateSecretKey,
    getPublicKey,
    finalizeEvent,
    verifyEvent
} from 'nostr-tools';
import { SimplePool } from 'nostr-tools';
import { nip19 } from 'nostr-tools';




const privateKey = generateSecretKey();
const publicKey = getPublicKey(privateKey);

console.log("秘密鍵 (Hex):", privateKey);
console.log("公開鍵 (Hex):", publicKey);

const npub = nip19.npubEncode(publicKey);
console.log("公開鍵 (Npub):", npub);

const pool = new SimplePool();
const relays = ['wss://relay.damus.io', 'wss://relay.snort.social', 'wss://nostr.wine'];

const messageContainer = document.getElementById('messages') as HTMLDivElement;


const sub = pool.subscribeMany(
    relays,
    [
        {
            kinds: [1],
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