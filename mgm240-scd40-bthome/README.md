# MGM240 SCD40 BTHome BLE Air Quality Monitor

Silicon Labs **MGM240** 모듈로 Sensirion **SCD40** 공기질 센서의 데이터를  
**BTHome v2** 포맷의 BLE 비연결 광고(advertising)로 송출합니다.  
Home Assistant가 자동 인식합니다.

## 하드웨어

| 부품 | 파트번호 |
|------|---------|
| BLE 모듈 | Silicon Labs MGM240PA22VNA (EFR32MG24 기반) |
| 공기질 센서 | Sensirion SCD40 (CO₂ / 온도 / 습도) |

## 배선 (SCD40 ↔ MGM240)

| SCD40 핀 | MGM240 핀 | 설명 |
|----------|-----------|------|
| VDD | 3.3 V | 전원 |
| GND | GND | 그라운드 |
| SDA | PC00 | I²C 데이터 |
| SCL | PC01 | I²C 클럭 |
| SEL | GND | I²C 모드 선택 |

> 핀 변경 시 `config/sl_i2cspm_sensor_config.h`를 수정하세요.

## 측정 항목

| 항목 | BTHome Object ID | 해상도 | 단위 |
|------|-----------------|--------|------|
| 온도 | 0x02 | 0.01 | °C |
| 습도 | 0x03 | 0.01 | % |
| CO₂ | 0x12 | 1 | ppm |

- 광고 주기: **100 ms**  
- 측정 갱신 주기: **5 초** (SCD40 주기 측정 모드)

## BTHome v2 패킷 구조

```
Offset  Bytes   설명
──────────────────────────────────────────────
 0      02 01 06               Flags AD record
 3      0D                     Service Data length = 13
 4      16                     AD type: Service Data
 5      D2 FC                  BTHome UUID 0xFCD2 (LE)
 7      40                     Device info: v2, no encryption
 8      02 TL TH               Temperature (sint16 LE, ×0.01 °C)
11      03 HL HH               Humidity    (uint16 LE, ×0.01 %)
14      12 CL CH               CO₂         (uint16 LE, ppm)
                                            총 17 bytes
```

## 소프트웨어 빌드

1. **Simplicity Studio v5** 설치
2. **Gecko SDK 4.4.0** 이상 설치
3. 프로젝트 가져오기: `mgm240_scd40_bthome.slcp`
4. 보드/디바이스 선택 (예: MGM240 Explorer Kit SLTB010A)
5. 빌드 & 플래시

## 파일 구조

```
mgm240-scd40-bthome/
├── mgm240_scd40_bthome.slcp   Simplicity Studio 프로젝트 파일
├── gatt_configuration.btconf  GATT 설정
├── app.h / app.c              메인 애플리케이션 (BT 이벤트 루프)
├── scd40.h / scd40.c          SCD40 I²C 드라이버
├── bthome.h / bthome.c        BTHome v2 패킷 빌더
└── config/
    └── sl_i2cspm_sensor_config.h  I²C 핀/속도 설정
```

## Home Assistant 연동

1. HA에서 Bluetooth 통합 활성화
2. BTHome 통합 설치 (Settings → Integrations → Add → BTHome)
3. 디바이스 전원 인가 시 자동 검색됨

## 동작 흐름

```
부팅
  └─ BT Stack Boot 이벤트
       ├─ Advertiser set 생성
       ├─ SCD40 초기화 (stop → start periodic measurement)
       ├─ 초기 광고 시작 (값 = 0)
       └─ 5 초 주기 타이머 시작
             └─ 타이머 콜백 (매 5 초)
                  ├─ scd40_data_ready() 확인
                  ├─ scd40_read_measurement()
                  └─ BTHome 패킷 갱신 → 광고 데이터 업데이트
```

## 라이선스

MIT
