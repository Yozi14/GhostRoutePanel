# GhostRoutePanel 👻

Панель управления VPN/прокси-сервером на **C++20** — аналог [Hiddify Manager](https://github.com/hiddify/Hiddify-Manager) с расширенной аналитикой, мультитенантностью и CLI.

## Возможности

### Аналитика
| Фича | Описание |
|------|----------|
| **Карта подключений** | Трафик по странам через GeoIP (MaxMind GeoLite2) |
| **Почасовой график** | Пики и провалы нагрузки за 24 часа |
| **Топ пользователей** | Кто расходует больше всего трафика |
| **Алерты** | Перегруз CPU, исчерпан лимит, сертификат &lt; 7 дней |

### Для владельца
| Фича | Описание |
|------|----------|
| **Мультитенантность** | Один бинарник — много клиентов (`tenant-slug.domain.com` или `X-Tenant-ID`) |
| **CLI** | `ghostroute user add --name vasya --limit 100gb` |
| **Автообновление** | `ghostroute update` |
| **Health check** | Мониторинг Xray/Hysteria, автоперезапуск |

### Сервер (как Hiddify)
- Автоустановка зависимостей, Nginx, Certbot, Xray, Hysteria2
- Синхронизация пользователей в конфиги ядер
- Веб-UI с **тёмной и светлой** темой

## Быстрый старт (Linux)

```bash
cd GhostRoutePanel
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

cp config/panel.example.json config/panel.json
# отредактируйте пути и домен

./build/ghostroute-panel config/panel.json
# UI: http://127.0.0.1:8080
```

### CLI

```bash
./build/ghostroute tenant add --slug acme --name "ACME Corp"
./build/ghostroute user add --name vasya --limit 100gb --tenant acme
./build/ghostroute user list --tenant acme
./build/ghostroute update
./build/ghostroute health
```

## Сборка на Windows

```powershell
vcpkg install sqlite3:x64-windows
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Панель разворачивается на **Linux-сервере**; на Windows удобно собирать и разрабатывать UI.

## GeoIP

Скачайте [GeoLite2-Country](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data) и укажите путь в `config/panel.json`:

```json
"geoip": { "database_path": "./data/GeoLite2-Country.mmdb" }
```

Для полноценного lookup подключите `libmaxminddb` (сейчас заглушка `XX` без MMDB).

## Архитектура

```
ghostroute-panel (HTTP :8080)
├── ghostroute_core
│   ├── Database (SQLite, multitenant schema)
│   ├── AnalyticsService + GeoIpService
│   ├── AlertService / HealthService
│   ├── XrayManager / HysteriaManager
│   └── ProvisionService → scripts/*.sh
└── web/ (SPA dashboard)
```

## API (кратко)

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/api/analytics/dashboard` | Графики, страны, топ |
| GET | `/api/alerts` | Активные алерты |
| GET/POST/DELETE | `/api/users` | CRUD пользователей |
| PATCH | `/api/users/:name` | Вкл/выкл, лимит, устройства |
| POST | `/api/users/:name/reset-traffic` | Сброс использованного трафика |
| GET | `/api/users/:name/link` | Subscription-ссылка (шаблон) |
| GET | `/api/system/stats` | CPU, RAM, диск, uptime |
| GET | `/api/health` | Статус Xray/Hysteria |
| POST | `/api/provision/full` | Полная установка сервера |

## Новое в v0.2

- Карточки пользователей: вкл/выкл, сброс трафика, лимит устройств
- Мониторинг CPU / RAM / диск / uptime (как в Hiddify)
- Логотип «лис свободы» (freedom route)
- Экспорт пользователей JSON, копирование UUID и subscription-ссылки

## Дорожная карта

- [ ] **Qt/C++ клиент** для пользователей (после стабилизации панели)
- [ ] JWT-авторизация и суперадмин
- [ ] libmaxminddb для реального GeoIP
- [ ] Парсинг access.log Xray → ingest в аналитику + учёт device_count
- [ ] Telegram-бот для алертов
- [ ] Docker-образ one-liner

## Лицензия

MIT — используйте на свой страх и риск для легальных VPN/прокси в вашей юрисдикции.
