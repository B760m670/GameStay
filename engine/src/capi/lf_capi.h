/* C-интерфейс ядра Lanternfall для сервера (cgo). Строки на выходе выделены malloc — освобождать lf_free. */
#ifndef LF_CAPI_H
#define LF_CAPI_H
#ifdef __cplusplus
extern "C" {
#endif

/* Новая деревня в формате сохранения. */
char* lf_new_village(long long now_ms);
/* Проверить и привести сохранение к текущему моменту (таймеры стройки, добыча). NULL — сохранение битое. */
char* lf_tick_save(const char* save, long long now_ms);
/* Краткая сводка для сервера: "level L\nrating R\nlantern N\nore O\naether A\nshards S\n". */
char* lf_summary(const char* save);
/* Снимок базы для атаки: строки "b ..." формата записи боя, с долей добычи lootFrac. */
char* lf_snapshot(const char* save, double loot_frac);
/* Проверить атаку: запись боя от клиента против снимка базы и сохранения атакующего.
   Успех — текст итога ("stars N\npct N\nore N\naether N\nshards N\nrating N\nwin 0|1\n"),
   *attacker_out — новое сохранение атакующего. Ошибка — NULL и *error с причиной. */
char* lf_verify_attack(const char* attacker_save, const char* snapshot, const char* replay, int expected_seed,
                       long long now_ms, char** attacker_out, char** error);
/* Применить итог атаки к защитнику: снимает добычу, меняет рейтинг. Возвращает новое сохранение. */
char* lf_apply_defense(const char* defender_save, long long loot_ore, long long loot_aether, int stars, int win, int* rating_delta);
/* Для тестов и ботов: честная запись боя — вся армия атакующего высаживается по краю карты. */
char* lf_auto_replay(const char* attacker_save, const char* snapshot, int seed, long long now_ms);
void lf_free(void* p);

#ifdef __cplusplus
}
#endif
#endif
