# Промпты сгенерированных построек (Higgsfield z_image, 1:1)

Общая обёртка (подставляется `<описание>`):

> Single isometric game building asset for a stylized fantasy strategy game, true 2:1 isometric projection viewed from the
> front corner, hand-painted 3D cartoon style like a premium mobile strategy game, lit from the top-left, rich detail, crisp
> dark outlines. Centered in frame with wide empty margins on all sides, the entire building and its base fully visible.
> `<описание>` The building sits on a small square patch of packed brown dirt with a few stones and green grass tufts.
> The background is one perfectly uniform flat pure magenta color #FF00FF everywhere, no gradient, no vignette, no glow on
> background, no shadows on background, no text.

Правила: в самой постройке никакого пурпурного/розового (фон вырезается по нему); Эфир — голубой/бирюзовый,
призма — золотисто-белая. `art/gen_sprite.py` предупреждает, если фон вышел не пурпурным — тогда перегенерировать.

Сборка спрайта: `art/fetch_gen.sh <имя>_t<ступень> <url> <клеток>`.

| Спрайт | Клеток | Описание |
|---|---|---|
| lantern_t2 | 4 | tall square stone lantern tower, brass brazier with a big golden flame on top, lit windows, barrels, street lamp |
| barracks_t1 | 3 | timber-framed barracks house with red tile roof, lit windows, spear rack, training dummy |
| oreMine_t1 | 3 | mine entrance in a grey rock with timber frame, hanging lantern, mine cart full of glowing red-orange ore |
| beacon_t1 | 2 | small stone beacon post: short round tower of light grey stone with brass bands, big brass lantern cage with golden flame |
