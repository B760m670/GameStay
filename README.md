# Lanternfall — Хранители Мглы

Мобильная стратегия о свете и Мгле (iOS, Unity). Диздок: [`docs/GDD.md`](docs/GDD.md).

## Структура

| Папка | Что внутри |
|---|---|
| `unity/` | Unity-проект (Unity 6000.3). `Assets/Game/Core` — игровое ядро на чистом C# без UnityEngine: деревня, детерминированная симуляция боя, генерация баз, кампания. |
| `core-tests/` | Тесты ядра на xUnit — запускаются без Unity: `dotnet test core-tests` |
| `distribution/` | Иконка и генератор источника для LiveContainer |
| `prototype-web/` | Первый браузерный прототип механики (для справки) |
| `.github/workflows/build.yml` | CI: тесты → Unity → Xcode → неподписанный `.ipa` в Releases |

## Установка на iPhone через LiveContainer

После каждой сборки в `main` GitHub Actions публикует релиз с `Lanternfall.ipa` и `source.json`.

- Источник (AltStore-формат): `https://github.com/B760m670/GameStay/releases/latest/download/source.json`
- Или напрямую `.ipa` из последнего релиза: `https://github.com/B760m670/GameStay/releases/latest/download/Lanternfall.ipa`

## Секреты для сборки iOS

В Settings → Secrets and variables → Actions нужны `UNITY_EMAIL`, `UNITY_PASSWORD`, `UNITY_LICENSE`
(содержимое файла лицензии Unity Personal `.ulf`). Без них CI запускает только тесты ядра.
