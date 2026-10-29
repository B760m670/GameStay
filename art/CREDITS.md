# Источники графики

3D-модели — наборы [Kenney](https://kenney.nl) (лицензия CC0 1.0, свободно для коммерческого использования):
Castle Kit, Fantasy Town Kit 2.0, Tower Defense Kit, Survival Kit, Graveyard Kit, Mini Characters.

Модели не хранятся в репозитории: `art/fetch_assets.sh` скачивает их, `art/blender/render_sprites.py` рендерит в спрайты.

Портреты персонажей (`art/source/portraits`) и заставка (`engine/assets/art/splash.jpg`) сгенерированы
в Higgsfield (модель Z Image) по описаниям персонажей игры.

## Poly Haven (CC0)

Фотосканы моделей и фототекстуры для руин главного здания: mossy_stone_wall, forest_ground_04, rock_moss_set_01/02,
dead_tree_trunk, tree_stump_01, bark_debris_01, fir_sapling, fern_02, grass_medium_01 — https://polyhaven.com, лицензия CC0.
Загружаются скриптом art/fetch_polyhaven.py.

## MakeHuman (CC0) и конь (CC-BY)

Люди в отрядах собраны из данных MakeHuman: базовая сетка, макро-цели, риг default, одежда и волосы из system assets —
лицензия CC0 (https://static.makehumancommunity.org/makehuman/license.html). Загружаются скриптом
art/fetch_makehuman.py (только файлы данных, код аддона MPFB не используется).

Конь всадника — цель формы `jaldmic_horse` из MakeHuman system assets, автор JALdMIC, лицензия CC-BY 4.0
(https://creativecommons.org/licenses/by/4.0/). Изменено: стилизация пропорций и материалов, процедурная анимация.

Материалы зданий, людей и природы — процедурные (art/blender/toon_kit.py), без фототекстур.
