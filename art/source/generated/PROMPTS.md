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
| lantern_t1 | 4 | tall square stone lantern tower, brass brazier with a big golden flame on top, lit windows, barrels, street lamp |
| barracks_t1 | 3 | timber-framed barracks house with red tile roof, lit windows, spear rack, training dummy |
| oreMine_t1 | 3 | mine entrance in a grey rock with timber frame, hanging lantern, mine cart full of glowing red-orange ore |
| beacon_t1 | 2 | small stone beacon post: short round tower of light grey stone with brass bands, big brass lantern cage with golden flame |
| aetherWell_t1 | 3 | mystical round stone well with a small wooden shingle roof and crank, glowing turquoise-cyan aether mist, cyan crystals, brass bucket |
| oreStore_t1 | 3 | sturdy ore storehouse: timber walls on stone foundation, iron-banded doors, plank roof, crates and piles of glowing red-orange ore |
| aetherStore_t1 | 3 | round reservoir tank of brass bands and thick glass on a stone plinth, glowing turquoise-cyan liquid aether, copper pipes, brass dome lid |
| camp_t1 | 3 | army camp: stone ring campfire, two beige/red striped canvas tents, weapon rack, banner with golden lantern emblem |
| builderHut_t1 | 2 | tiny cozy builder's hut, golden thatched roof, chimney, round door, workbench with hammer and saw, planks, hanging lantern |
| ballista_t1 | 3 | huge wooden crossbow with iron fittings on a rotating timber platform atop a low round stone tower, bundles of bolts |
| brazier_t1 | 2 | large black iron bowl with spikes on a short carved stone pedestal, roaring orange flames, brass rings, coal |
| catapult_t1 | 3 | wooden catapult with iron bolts and a rope sling on a square stone platform, pile of boulders, coil of rope |
| prism_t1 | 2 | tall faceted golden-white crystal held by curved brass claws on a carved stone pedestal, shining warm light |
| workshop_t1 | 3 | blacksmith forge: timber-framed stone building, red tile roof, tall chimney with smoke, glowing forge, anvil, quenching barrel |

Промпт в длинной форме («perfectly uniform … no vignette») дважды падал в очереди сервиса — используется короткая форма выше
(«Centered with wide margins … Isolated on a plain flat solid magenta background (#FF00FF), no ground beyond the footprint …»).
