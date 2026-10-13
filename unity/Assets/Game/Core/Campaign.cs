// Кампания «Хроника Мглы»: 12 глав, сюжет и записи кодекса.
using System.Collections.Generic;

namespace Lanternfall.Core
{
    public enum Speaker { Narrator, Vira, Keeper, Haze, King }
    public struct Line { public Speaker Who; public string Text; public Line(Speaker w, string t) { Who = w; Text = t; } }
    public sealed class Mission
    {
        public string Id, Title, Codex;
        public int LL, Seed, LevelBias, RewardOre, RewardAether, RewardShards;
        public float Richness = 1f;
        public Dictionary<BType, int> Counts;
        public Line[] Intro, Outro;
    }
    public sealed class CodexEntry { public string Title, Text; }

    public static class Campaign
    {
        public static readonly Dictionary<Speaker, (string name, string color)> Speakers = new Dictionary<Speaker, (string, string)>
        {
            [Speaker.Vira] = ("Вира, старая фонарщица", "#ffd28a"),
            [Speaker.Keeper] = ("Хранитель", "#cfe8ff"),
            [Speaker.Haze] = ("Мгла", "#b59ae0"),
            [Speaker.King] = ("Король Пепла", "#e08a8a"),
            [Speaker.Narrator] = ("", "#d8d0c0"),
        };

        public static readonly Line[] Intro = new[] {
                new Line(Speaker.Narrator, "Триста лет Великий Маяк на вершине Аргентума держал Мглу за краем мира."),
                new Line(Speaker.Narrator, "Однажды ночью Маяк раскололся. Осколки разлетелись по земле, и Мгла хлынула в долины, стирая всё, чего касалась."),
                new Line(Speaker.Vira, "Очнулся? Хорошо. В твоих руках тлел осколок — я вставила его в старый Фонарь. Пока он горит, Мгла не войдёт в круг."),
                new Line(Speaker.Vira, "Строй только там, куда падает свет. Ставь Светочи — они раздвигают тьму. И помни: башня без света — просто камень."),
                new Line(Speaker.Vira, "Ночью Мгла придёт проверить нас. А днём… днём мы пойдём за другими осколками. Их держат Угасшие — Хранители, чьи сердца остыли."),
        };

        public static readonly Mission[] Missions =
        {
            new Mission
            {
                Id = "m1", Title = "Хижина у брода", LL = 1, Seed = 101, Richness = 0.5f, LevelBias = 0, Codex = "ford",
                RewardOre = 500, RewardAether = 500, RewardShards = 3,
                Counts = new Dictionary<BType, int> { [BType.Beacon] = 0, [BType.OreMine] = 1, [BType.AetherWell] = 1, [BType.Barracks] = 0, [BType.Camp] = 0, [BType.BuilderHut] = 1, [BType.Ballista] = 0, [BType.Wall] = 0, [BType.AetherStore] = 1, [BType.OreStore] = 1 },
                Intro = new[] {
                    new Line(Speaker.Vira, "У брода засел мародёр с краденой искрой. Ни стен, ни башен — хорошее место, чтобы научиться."),
                    new Line(Speaker.Vira, "Высаживай Факельщиков во тьме — туда, куда не падает его свет. Ближе к свету подойти не дадут."),
                },
                Outro = new[] {
                    new Line(Speaker.Vira, "Неплохо для первого раза. Искра — наша. Видишь, как она потянулась к нашему Фонарю?"),
                },
            },
            new Mission
            {
                Id = "m2", Title = "Тлеющий хутор", LL = 1, Seed = 202, Richness = 0.8f, LevelBias = 0, Codex = "beacons",
                RewardOre = 800, RewardAether = 800, RewardShards = 3,
                Counts = new Dictionary<BType, int> { [BType.Beacon] = 1, [BType.Wall] = 0 },
                Intro = new[] {
                    new Line(Speaker.Vira, "Хутор Угасших. У них есть Светоч — видишь, как он вытягивает свет за пределы Фонаря?"),
                    new Line(Speaker.Vira, "Сбей Светоч — и его сектор погаснет. Арбалет во тьме не выстрелит, а ты сможешь высаживаться ближе."),
                },
                Outro = new[] {
                    new Line(Speaker.Keeper, "Светоч погас — и башня ослепла. Так вот как это работает."),
                },
            },
            new Mission
            {
                Id = "m3", Title = "Застава Угасших", LL = 2, Seed = 303, Richness = 1f, LevelBias = 0, Codex = "king",
                RewardOre = 1500, RewardAether = 1500, RewardShards = 4,
                Intro = new[] {
                    new Line(Speaker.Vira, "Застава на старом тракте. За стенами — Жаровня. Не пускай толпу под её пламя."),
                    new Line(Speaker.Vira, "У тебя есть «Затмение». Брось его на часть базы — свет там погаснет на десять вдохов."),
                },
                Outro = new[] {
                    new Line(Speaker.King, "…Ещё один Хранитель тащит осколки к своему огарку. Беги, пока можешь, светлячок."),
                },
            },
            new Mission
            {
                Id = "m4", Title = "Мельница на краю Мглы", LL = 2, Seed = 404, Richness = 1.2f, LevelBias = 0, Codex = "aether",
                RewardOre = 2500, RewardAether = 2500, RewardShards = 4,
                Intro = new[] {
                    new Line(Speaker.Vira, "Мельница перемалывает туман в Эфир. Угасшие охраняют её двумя Светочами."),
                    new Line(Speaker.Keeper, "Вира… кто такой Король Пепла?"),
                    new Line(Speaker.Vira, "Первый из Угасших. Говорят, он был лучшим Хранителем Маяка. Пока не погас."),
                },
                Outro = new[] {
                    new Line(Speaker.Vira, "Слышишь, как шепчет Эфир в бочках? Не прислушивайся. Никогда не прислушивайся."),
                },
            },
            new Mission
            {
                Id = "m5", Title = "Острог Серых Шлемов", LL = 3, Seed = 505, Richness = 1f, LevelBias = 0, Codex = "helms",
                RewardOre = 5000, RewardAether = 5000, RewardShards = 5,
                Intro = new[] {
                    new Line(Speaker.Vira, "Серые Шлемы продали свои огни Королю за обещание вечной ночи без страха."),
                    new Line(Speaker.Vira, "Их Катапульта бьёт далеко, но слепа вблизи. Щитоносцы примут удар на себя."),
                },
                Outro = new[] {
                    new Line(Speaker.Keeper, "В остроге нашли карту. На ней отмечена Библиотека Тишины — и подпись: «Здесь знали правду о свете»."),
                },
            },
            new Mission
            {
                Id = "m6", Title = "Библиотека Тишины", LL = 3, Seed = 606, Richness = 1.3f, LevelBias = 0, Codex = "truth",
                RewardOre = 6000, RewardAether = 6000, RewardShards = 6,
                Intro = new[] {
                    new Line(Speaker.Vira, "Не ходи туда. Некоторые книги лучше оставить Мгле."),
                    new Line(Speaker.Keeper, "Я должен знать, за что мы воюем."),
                },
                Outro = new[] {
                    new Line(Speaker.Narrator, "В последнем зале — дневник смотрителя Маяка. Чернила почти выцвели."),
                    new Line(Speaker.Narrator, "«Эфир — это память. Маяк горит, сжигая то, что люди забыли. Мгла — пепел этих воспоминаний. Она не хочет убивать. Она хочет вернуться домой»."),
                    new Line(Speaker.Vira, "…Теперь ты знаешь. Я знала всегда. Прости."),
                },
            },
            new Mission
            {
                Id = "m7", Title = "Перевал Сов", LL = 4, Seed = 707, Richness = 1f, LevelBias = 0, Codex = "owls",
                RewardOre = 12000, RewardAether = 12000, RewardShards = 6,
                Intro = new[] {
                    new Line(Speaker.Vira, "Совиные всадники Перевала когда-то возили письма между Маяками. Теперь их гнёзда охраняет Призма."),
                    new Line(Speaker.Vira, "Призма разгорается, пока держит цель. Не давай ей сосредоточиться на одном бойце."),
                },
                Outro = new[] {
                    new Line(Speaker.Keeper, "Совы помнят свет. Они полетят с нами — когда казарма будет готова их принять."),
                },
            },
            new Mission
            {
                Id = "m8", Title = "Город-Фонарь Лумен", LL = 4, Seed = 808, Richness = 1.4f, LevelBias = 0, Codex = "lumen",
                RewardOre = 15000, RewardAether = 15000, RewardShards = 7,
                Intro = new[] {
                    new Line(Speaker.Haze, "Хранитель… ты помнишь, как звали твою мать?"),
                    new Line(Speaker.Keeper, "…Нет. Не помню."),
                    new Line(Speaker.Haze, "Мы помним. Отпусти свет — и мы вернём."),
                    new Line(Speaker.Vira, "Не слушай! Лумен — последний город, где Угасшие держат сразу три осколка. Вперёд."),
                },
                Outro = new[] {
                    new Line(Speaker.Vira, "Три осколка. Фонарь гудит, как улей. Мы близко к Королю."),
                },
            },
            new Mission
            {
                Id = "m9", Title = "Чёрное Зеркало", LL = 5, Seed = 909, Richness = 1f, LevelBias = 0, Codex = "mirror",
                RewardOre = 25000, RewardAether = 25000, RewardShards = 8,
                Intro = new[] {
                    new Line(Speaker.Vira, "Озеро-зеркало. Король расставил здесь Призмы — свет отражается от воды и бьёт вдвое злее."),
                    new Line(Speaker.Vira, "Тенекрады теперь с нами. Пусти их по тьме к Светочам — их не заметят, пока не станет поздно."),
                },
                Outro = new[] {
                    new Line(Speaker.King, "Ты гасишь мои огни и зажигаешь свои. Ты думаешь, это разные огни, светлячок?"),
                },
            },
            new Mission
            {
                Id = "m10", Title = "Цитадель Короля Пепла", LL = 5, Seed = 1010, Richness = 1.5f, LevelBias = 0, Codex = "ashking",
                RewardOre = 30000, RewardAether = 30000, RewardShards = 10,
                Intro = new[] {
                    new Line(Speaker.King, "Я был Хранителем Маяка. Я жёг память людей, чтобы держать свет. Миллионы забытых имён."),
                    new Line(Speaker.King, "Я расколол его сам. И угас. Потому что больше не мог."),
                    new Line(Speaker.Vira, "Он лжёт… или нет. Неважно. Его осколок — самый крупный. Он нужен нам."),
                },
                Outro = new[] {
                    new Line(Speaker.King, "Ты взял осколок. Значит, теперь выбор — твой. Поднимись на Аргентум."),
                    new Line(Speaker.Narrator, "Король Пепла рассыпался серой пылью. Мгла вокруг цитадели на миг стала прозрачной, как утренний туман."),
                },
            },
            new Mission
            {
                Id = "m11", Title = "Подножие Аргентума", LL = 6, Seed = 1111, Richness = 1f, LevelBias = 0, Codex = "argentum",
                RewardOre = 45000, RewardAether = 45000, RewardShards = 10,
                Intro = new[] {
                    new Line(Speaker.Vira, "Последние Угасшие стерегут тропу на гору. Они уже не служат Королю. Они просто боятся."),
                    new Line(Speaker.Keeper, "Чего?"),
                    new Line(Speaker.Vira, "Того, что ты выберешь."),
                },
                Outro = new[] {
                    new Line(Speaker.Vira, "Тропа открыта. Наверху — Сердце Маяка."),
                },
            },
            new Mission
            {
                Id = "m12", Title = "Сердце Маяка", LL = 6, Seed = 1212, Richness = 2f, LevelBias = 1, Codex = "heart",
                RewardOre = 80000, RewardAether = 80000, RewardShards = 25,
                Intro = new[] {
                    new Line(Speaker.Haze, "Ты принёс все осколки. Разожги Маяк — и мы снова станем пеплом. Или открой его — и мы вернёмся к вам."),
                    new Line(Speaker.Vira, "Сердце охраняет то, что осталось от гвардии Маяка. Пробейся — и реши."),
                },
                Outro = new[] {
                    new Line(Speaker.Narrator, "Сердце Маяка лежит перед тобой — холодное, как зимняя звезда."),
                    new Line(Speaker.Narrator, "Разжечь его — значит снова сжигать память мира ради света. Открыть — значит впустить Мглу и вернуть людям забытое."),
                    new Line(Speaker.Vira, "Решай, Хранитель. Но не сегодня. Сегодня — просто посмотри на рассвет."),
                    new Line(Speaker.Narrator, "— Конец первого сезона —"),
                },
            },
        };

        public static readonly Dictionary<string, CodexEntry> Codex = new Dictionary<string, CodexEntry>
        {
            ["intro"] = new CodexEntry { Title = "Раскол Маяка", Text = "Триста лет назад Великий Маяк на Аргентуме раскололся. Осколки разлетелись, Мгла затопила долины. Каждый, кто нашёл осколок и разжёг Фонарь, зовётся Хранителем." },
            ["ford"] = new CodexEntry { Title = "Искры", Text = "Мелкие частицы осколков. Тянутся к ближайшему сильному свету — так Фонари растут, поглощая чужие искры." },
            ["beacons"] = new CodexEntry { Title = "Светочи", Text = "Фонарь не может светить далеко. Светочи — зеркальные столпы, разносящие его свет. Погаси Светоч — и всё, что он питал, ослепнет." },
            ["king"] = new CodexEntry { Title = "Угасшие", Text = "Хранители, чьи Фонари остыли. Они не мертвы — они перестали бояться тьмы. Ими правит Король Пепла." },
            ["aether"] = new CodexEntry { Title = "Эфир", Text = "Светящийся конденсат тумана. Им кормят армии и строят дома. Кто прислушается к Эфиру слишком долго — слышит голоса." },
            ["helms"] = new CodexEntry { Title = "Серые Шлемы", Text = "Наёмники, променявшие свои огни на обещание ночи без страха. Носят шлемы без прорезей для глаз." },
            ["truth"] = new CodexEntry { Title = "Дневник смотрителя", Text = "«Эфир — это память. Маяк горит, сжигая то, что люди забыли. Мгла — пепел этих воспоминаний. Она не хочет убивать. Она хочет вернуться домой»." },
            ["owls"] = new CodexEntry { Title = "Совиные всадники", Text = "Почтальоны эпохи Маяка. Совы видят в темноте и помнят каждый огонь, над которым пролетали." },
            ["lumen"] = new CodexEntry { Title = "Лумен", Text = "Когда-то — город тысячи фонарей. Теперь — крепость Угасших, где осколки держат в клетках, как птиц." },
            ["mirror"] = new CodexEntry { Title = "Чёрное Зеркало", Text = "Озеро, в котором отражается не небо, а то, что ты забыл." },
            ["ashking"] = new CodexEntry { Title = "Король Пепла", Text = "Последний смотритель Великого Маяка. Расколол его сам, не выдержав груза сожжённых воспоминаний." },
            ["argentum"] = new CodexEntry { Title = "Аргентум", Text = "Серебряная гора. На её вершине стоял Маяк. Мгла обтекает её, как вода — камень." },
            ["heart"] = new CodexEntry { Title = "Сердце Маяка", Text = "Все осколки вместе. Выбор между светом и памятью ждёт тебя в следующем сезоне." },
        };

        public static bool Unlocked(VillageState S, int idx) => idx == 0 || S.Stars(Missions[idx - 1].Id) > 0;
    }
}
