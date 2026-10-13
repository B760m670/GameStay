// Сборка из командной строки / GitHub Actions: Unity -batchmode -executeMethod Lanternfall.Editor.BuildScript.BuildIOS
using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace Lanternfall.Editor
{
    public static class BuildScript
    {
        const string ScenePath = "Assets/Game/Scenes/Main.unity";
        public const string BundleId = "com.gamestay.lanternfall";

        public static void BuildIOS()
        {
            EnsureScene();
            ConfigurePlayer();
            string path = Arg("-customBuildPath") ?? Env("BUILD_PATH", "build/iOS/iOS");
            var report = BuildPipeline.BuildPlayer(new BuildPlayerOptions
            {
                scenes = new[] { ScenePath },
                locationPathName = path,
                target = BuildTarget.iOS,
                options = BuildOptions.None,
            });
            Debug.Log($"[Lanternfall] Build {report.summary.result}, size {report.summary.totalSize}, path {path}");
            if (report.summary.result != BuildResult.Succeeded) EditorApplication.Exit(1);
        }

        static void ConfigurePlayer()
        {
            PlayerSettings.companyName = "GameStay";
            PlayerSettings.productName = "Lanternfall";
            PlayerSettings.SetApplicationIdentifier(NamedBuildTarget.iOS, BundleId);
            PlayerSettings.bundleVersion = Arg("-appVersion") ?? Env("APP_VERSION", "0.1.0");
            PlayerSettings.iOS.buildNumber = Arg("-buildNumber") ?? Env("BUILD_NUMBER", "1");
            PlayerSettings.iOS.targetOSVersionString = "15.0";
            PlayerSettings.iOS.appleEnableAutomaticSigning = false;
            PlayerSettings.SetScriptingBackend(NamedBuildTarget.iOS, ScriptingImplementation.IL2CPP);
            // Мобильная стратегия: только альбомная ориентация, без статус-бара.
            PlayerSettings.defaultInterfaceOrientation = UIOrientation.AutoRotation;
            PlayerSettings.allowedAutorotateToLandscapeLeft = true;
            PlayerSettings.allowedAutorotateToLandscapeRight = true;
            PlayerSettings.allowedAutorotateToPortrait = false;
            PlayerSettings.allowedAutorotateToPortraitUpsideDown = false;
            PlayerSettings.statusBarHidden = true;
        }

        static void EnsureScene()
        {
            if (File.Exists(ScenePath)) return;
            Directory.CreateDirectory(Path.GetDirectoryName(ScenePath));
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            new GameObject("Bootstrap").AddComponent<Lanternfall.Bootstrap>();
            EditorSceneManager.SaveScene(scene, ScenePath);
        }

        static string Arg(string name)
        {
            var args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length - 1; i++) if (args[i] == name) return args[i + 1];
            return null;
        }

        static string Env(string name, string fallback)
        {
            var v = Environment.GetEnvironmentVariable(name);
            return string.IsNullOrEmpty(v) ? fallback : v;
        }
    }
}
