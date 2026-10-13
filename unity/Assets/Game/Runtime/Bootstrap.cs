// Временная точка входа: проверяет, что ядро работает на устройстве, и хранит сохранение.
// Полноценный клиент (изометрия, спрайты, UI) строится поверх этого после активации лицензии Unity.
using System.IO;
using Lanternfall.Core;
using UnityEngine;

namespace Lanternfall
{
    public class Bootstrap : MonoBehaviour
    {
        VillageState state;
        string SavePath => Path.Combine(Application.persistentDataPath, "save.json");

        static long Now => System.DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

        void Awake()
        {
            Application.targetFrameRate = 60;
            Screen.sleepTimeout = SleepTimeout.NeverSleep;
            if (Camera.main == null)
            {
                var cam = new GameObject("Main Camera").AddComponent<Camera>();
                cam.tag = "MainCamera";
                cam.orthographic = true;
                cam.clearFlags = CameraClearFlags.SolidColor;
                cam.backgroundColor = new Color(0.05f, 0.04f, 0.11f);
            }
            state = Load() ?? Village.Create(Now);
            Village.Tick(state, Now);
        }

        VillageState Load()
        {
            try
            {
                if (!File.Exists(SavePath)) return null;
                var s = JsonUtility.FromJson<VillageState>(File.ReadAllText(SavePath));
                return s != null && s.version == VillageState.SaveVersion ? s : null;
            }
            catch { return null; }
        }

        void Save() => File.WriteAllText(SavePath, JsonUtility.ToJson(state));
        void OnApplicationPause(bool paused) { if (paused) Save(); }
        void OnApplicationQuit() => Save();

        void OnGUI()
        {
            var s = Mathf.Max(1f, Screen.height / 540f);
            var title = new GUIStyle(GUI.skin.label) { fontSize = (int)(48 * s), alignment = TextAnchor.MiddleCenter };
            title.normal.textColor = new Color(1f, 0.85f, 0.42f);
            var body = new GUIStyle(GUI.skin.label) { fontSize = (int)(18 * s), alignment = TextAnchor.MiddleCenter };
            body.normal.textColor = new Color(0.8f, 0.75f, 0.9f);
            GUI.Label(new Rect(0, Screen.height * 0.3f, Screen.width, 80 * s), "LANTERNFALL", title);
            GUI.Label(new Rect(0, Screen.height * 0.3f + 80 * s, Screen.width, 40 * s), "Хранители Мглы", body);
            GUI.Label(new Rect(0, Screen.height * 0.3f + 130 * s, Screen.width, 40 * s),
                $"Сборка {Application.version} · Фонарь {Village.LanternLevel(state)} · построек: {state.buildings.Count}", body);
        }
    }
}
