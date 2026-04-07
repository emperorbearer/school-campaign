# 학교문화 책임규약 홍보 및 학교폭력예방 캠페인 시스템

이 프로젝트는 라즈베리파이 4와 18.5인치 모니터를 활용하여 캠페인 포스터 이미지들을 슬라이드쇼로 순환하며 보여주는 디지털 사이니지 시스템입니다.

## 기술 스택
- **프론트엔드**: Svelte (Vite 기반), 세련된 애니메이션과 다크 모드 적용
- **백엔드**: Python Django & Django Ninja
- **데이터베이스**: SQLite

## 실행 방법

가장 간편한 방법은 제공된 `run.ps1` 스크립트를 실행하는 것입니다.
프로젝트 최상위 디렉터리에서 다음 명령어를 실행하면 됩니다:
```powershell
.\run.ps1
```

수동으로 실행하려면 다음을 따르세요:

### 1. 백엔드 실행
새 터미널을 열고 다음 안내를 따릅니다:
```bash
cd backend
# 가상환경 활성화 (Windows)
.\venv\Scripts\activate
# (Linux/Mac의 경우: source venv/bin/activate)

# 서버 실행
python manage.py runserver
```

**포스터 이미지 관리**
- 주소: `http://127.0.0.1:8000/admin`
- 관리자 계정 초기 정보:
  - 아이디: `admin`
  - 비밀번호: `admin`
이곳에 접속해 `api` 앱의 `포스터` 메뉴에서 새로운 캠페인 이미지를 업로드할 수 있습니다.

### 2. 프론트엔드 실행
새 터미널을 열고 다음 안내를 따릅니다:
```bash
cd frontend
# 의존성 설치가 안 되어 있다면: npm install
npm run dev -- --host
```

**디지털 사이니지 재생 (라즈베리파이 풀스크린)**
- 웹 브라우저 (크롬 등)에서 `http://127.0.0.1:5173` 으로 접속합니다.
- `F11` (또는 브라우저 전체화면 단축키)를 눌러 슬라이드쇼를 플레이합니다.

## 시스템 기능 (System Features)
- **우수한 디자인 (Dynamic & Premium Aesthetics)**
  세련된 전환 애니메이션(`app.css`), 자동화된 진행바 표시줄(Progress bar tracker)이 화면에서 시선을 유도하며, 캠페인 설명이 투명하게 모던한 글래스모피즘(Glassmorphism) 효과로 겹쳐 보여줍니다.
- **REST API**
  Django Ninja 기반의 `http://127.0.0.1:8000/api/posters` 로 데이터를 주고받습니다.
