# IoT水やりシステム フローチャート

## システム概要
ESP8266を使用した自動水やりシステムで、BME280センサーで温湿度・気圧を測定し、土壌水分センサーで土の乾燥度を監視して自動給水を行います。

## メインフローチャート

```mermaid
flowchart TD
    A[開始] --> B[初期化<br/>WiFi・MQTT・センサー設定]
    B --> C[メインループ開始]
    C --> D{5秒経過?}
    D -->|No| C
    D -->|Yes| E[センサーデータ読み取り]
    E --> F[ポンプ制御処理]
    F --> G[データ送信]
    G --> C
```

## WiFi接続フロー

```mermaid
flowchart TD
    A[setupWifi開始] --> B[WiFi.begin実行]
    B --> C{WiFi接続確認}
    C -->|未接続| D[500ms待機]
    D --> E[接続状況をシリアル出力]
    E --> C
    C -->|接続済み| F[IPアドレスをシリアル出力]
    F --> G[setupWifi終了]
```

## MQTT接続フロー

```mermaid
flowchart TD
    A[reconnect開始] --> B{client接続確認}
    B -->|未接続| C[接続試行]
    C --> D{接続成功?}
    D -->|失敗| E[エラーコード出力]
    E --> F[5秒待機]
    F --> B
    D -->|成功| G[接続状態をMQTT送信]
    G --> H[トピック購読設定]
    H --> I[reconnect終了]
    B -->|接続済み| I
```

## MQTTコールバック処理フロー

```mermaid
flowchart TD
    A[callback関数開始] --> B[受信トピック確認]
    B --> C{ポンプ制御トピック?}
    C -->|Yes| D{payload = '1'?}
    D -->|Yes| E[ポンプON<br/>pumpRunning = true]
    D -->|No| F[ポンプOFF<br/>pumpRunning = false]
    E --> G[ポンプ状態をMQTT送信]
    F --> G
    
    C -->|No| H{閾値更新トピック?}
    H -->|Yes| I[新しい閾値を解析]
    I --> J{閾値が有効範囲?<br/>0 < threshold < 2000}
    J -->|Yes| K[soil_threshold更新]
    J -->|No| L[閾値更新をスキップ]
    K --> M[現在の閾値をMQTT送信]
    L --> N[callback終了]
    M --> N
    
    H -->|No| O{閾値取得要求トピック?}
    O -->|Yes| P[現在の閾値をMQTT送信]
    O -->|No| Q[その他のトピック処理]
    P --> N
    Q --> N
    G --> N
```

## ポンプ制御フロー

```mermaid
flowchart TD
    A[ポンプ制御開始] --> B{ポンプ稼働中?}
    B -->|Yes| C{3秒経過?}
    C -->|Yes| D[ポンプ停止<br/>pumpRunning = false]
    C -->|No| E[ポンプ継続]
    D --> F[ポンプ状態をMQTT送信]
    E --> F
    
    B -->|No| G{土壌が乾燥?<br/>soil_value > threshold}
    G -->|Yes| H[ポンプ開始<br/>pumpRunning = true]
    G -->|No| I[ポンプ停止状態維持]
    H --> J[開始時刻記録<br/>pumpStartTime = millis()]
    J --> K[ポンプ状態をMQTT送信]
    I --> L[ポンプ制御終了]
    F --> L
    K --> L
```

## センサーデータ読み取りフロー

```mermaid
flowchart TD
    A[センサーデータ読み取り開始] --> B[BME280: 温度読み取り]
    B --> C[BME280: 湿度読み取り]
    C --> D[BME280: 気圧読み取り]
    D --> E[土壌水分センサー読み取り<br/>soil_value = analogRead(SENSE_PIN)]
    E --> F[データをシリアル出力]
    F --> G[センサーデータ読み取り終了]
```

## データ送信フロー

```mermaid
flowchart TD
    A[データ送信開始] --> B[温度データをMQTT送信]
    B --> C[湿度データをMQTT送信]
    C --> D[気圧データをMQTT送信]
    D --> E[土壌水分データをMQTT送信]
    E --> F[データ送信終了]
```

## 主要変数

| 変数名 | 型 | 説明 |
|--------|----|----|
| `soil_value` | float | 土壌水分センサーの値 |
| `soil_threshold` | int | 土壌水分閾値（デフォルト: 1024） |
| `pumpRunning` | bool | ポンプ稼働状態フラグ |
| `pumpStartTime` | unsigned long | ポンプ開始時刻 |
| `pumpDuration` | const unsigned long | ポンプ稼働時間（3000ms = 3秒） |
| `readInterval` | const unsigned long | センサー読み取り間隔（5000ms = 5秒） |

## 主要機能

1. **自動水やり制御**: 土壌水分が閾値を超えると自動でポンプを3秒間稼働
2. **センサーデータ監視**: 温度、湿度、気圧、土壌水分を5秒間隔で測定
3. **MQTT通信**: センサーデータとポンプ状態をMQTTブローカーに送信
4. **リモート制御**: MQTT経由でポンプの手動制御と閾値設定が可能
5. **WiFi接続**: ESP8266のWiFi機能でネットワークに接続

## 使用センサー・デバイス

- **ESP8266**: WiFi通信とメイン制御
- **BME280**: 温度・湿度・気圧センサー（SPI通信）
- **土壌水分センサー**: アナログ入力（A0ピン）
- **給水ポンプ**: デジタル出力（HUMIピン） 
