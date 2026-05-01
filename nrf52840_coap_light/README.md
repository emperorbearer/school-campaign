# nRF52840 CoAP Light Button (NCS / Zephyr)

nRF Connect SDK 기반 저전력 CoAP 라이트 버튼 펌웨어입니다.  
OpenThread MTD-SED (Sleepy End Device) 모드로 동작하며 버튼을 누를 때만 라디오가 활성화됩니다.

## 기능

| 버튼 | 동작 |
|------|------|
| BTN1 | 라이트 ON (CoAP PUT `/light` payload=1) |
| BTN2 | 라이트 OFF (CoAP PUT `/light` payload=0) |
| BTN3 | 라이트 TOGGLE (CoAP PUT `/light` payload=2) |

- **클라이언트**: 버튼 입력 시 Thread 네트워크로 CoAP PUT 전송 (멀티캐스트 또는 유니캐스트)
- **서버**: CoAP PUT `/light` 수신 시 로컬 LED 제어
- LED1: 로컬 라이트 상태 표시
- LED2: 부팅 완료 표시 (300 ms)

## 저전력 구조

```
[ 버튼 인터럽트 ]──┐
                   ├──► k_sem_give ──► rx_thread 깨어남 ──► CoAP TX ──► 슬립
[ 수신 없음 ]──────┘    (k_sem_take → WFI 슬립)
```

- `CONFIG_OPENTHREAD_MTD_SED=y` — Sleepy End Device (라디오 기본 OFF)
- `CONFIG_OPENTHREAD_POLL_PERIOD=5000` — 부모 노드 폴링 간격 5 초
- Zephyr idle 스레드 → WFI → 약 **3–5 µA** (라디오 비활성 구간)
- CoAP 전송 1회 소요 전력: ~5 ms TX burst

## 빌드

```bash
# NCS v2.6.x 이상 권장
west build -b nrf52840dk/nrf52840 -- -DCONF_FILE=prj.conf

# 플래시
west flash
```

## Thread 네트워크 설정

`src/main.c` 상단의 상수를 네트워크 환경에 맞게 수정하세요:

```c
#define OT_CHANNEL       15       // 11–26
#define OT_PANID         0xABCD
#define OT_NETWORK_NAME  "CoAPLight"
static const uint8_t OT_NETWORK_KEY[16] = { ... };
```

CoAP 서버 주소를 유니캐스트로 고정하려면:

```c
#define COAP_SERVER_ADDR  "fd12:3456:789a::1"  // 실제 서버 주소
```

## 파일 구조

```
nrf52840_coap_light/
├── CMakeLists.txt
├── prj.conf                          # Kconfig (CoAP, Thread, PM)
├── boards/
│   ├── nrf52840dk_nrf52840.conf      # 보드 전용 Kconfig
│   └── nrf52840dk_nrf52840.overlay   # 핀 / USB 오버레이
└── src/
    ├── main.c                        # 진입점, OpenThread 초기화, 버튼 핸들러
    ├── coap_utils.c                  # CoAP 클라이언트/서버 구현
    └── coap_utils.h                  # API 헤더
```

## 소비 전류 (참고치)

| 상태 | 전류 |
|------|------|
| SED 슬립 (라디오 OFF) | ~3 µA |
| Thread 폴링 (5 s 주기) | ~0.5 mA 순간 |
| CoAP 전송 | ~7 mA × 5 ms |
| 평균 (CR2032 기준) | **< 10 µA** |
