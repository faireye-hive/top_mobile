cmake_minimum_required(VERSION 3.16)
project(top_gl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 05-AT: interface como biblioteca independente da cena 3D e do GLX.
# NpcDialogUI, QuestUI, inventario e widgets produzem somente dados/lotes;
# ActorSkinUIBridge permanece adaptador no executavel grafico.
add_library(top_ui STATIC
    Engine/ui/UIRenderer.cpp
    Engine/ui/HudComposer.cpp
    Engine/ui/UIManager.cpp
    Engine/ui/screens/ForgeWorkbenchUI.cpp
    Engine/ui/screens/MinimapUI.cpp
    Engine/ui/screens/GameHUD.cpp
    Engine/ui/screens/CharacterStatusUI.cpp
    Engine/ui/screens/QuestUI.cpp
    Engine/ui/screens/SkillsUI.cpp
    Engine/ui/screens/InventoryUI.cpp
    Engine/ui/screens/NpcDialogUI.cpp
    Engine/ui/screens/ShopUI.cpp
    Engine/ui/screens/BoatUI.cpp
    Engine/ui/screens/SystemUI.cpp
    Engine/ui/screens/CharacterSelectUI.cpp
    Engine/ui/screens/ShortcutBarUI.cpp
)
target_include_directories(top_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/ui
)

# 05-AW: regras puras dos itens compilam sem Play, NPC, UI ou OpenGL.
add_library(top_item STATIC Engine/src/item/ItemUseRules.cpp Engine/src/item/ItemChestRules.cpp Engine/src/item/ItemTicketRules.cpp Engine/src/item/ItemNewbieChestRules.cpp Engine/src/item/ItemInstantiationRules.cpp Engine/src/item/ItemRandomRewardRules.cpp Engine/src/item/ItemStatRules.cpp Engine/src/item/ItemProductionRules.cpp Engine/src/item/ItemGemVoucherRules.cpp)
target_include_directories(top_item PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

set(TOP_GAME_SOURCES
    Engine/src/top/MapFile.cpp
    Engine/src/top/AssetAudit.cpp
    Engine/src/top/PathFinder.cpp
    Engine/src/top/Image.cpp
    Engine/src/top/TexturePaths.cpp
    Engine/src/top/TerrainTable.cpp
    Engine/src/top/AreaLighting.cpp
    Engine/src/top/TerrainMesh.cpp
    Engine/src/top/ModelFile.cpp
    Engine/src/top/SceneWorld.cpp
    Engine/src/top/ActorSkin.cpp
    Engine/ui/ActorSkinUIBridge.cpp
    Engine/src/top/Cursor.cpp
    Engine/src/top/EffectSystem.cpp
    Engine/src/top/AudioSystem.cpp
    Engine/src/top/PortalSystem.cpp
    Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp
    Engine/src/top/MonsterSpawnSystem.cpp
    Engine/src/top/Play.cpp
    Engine/src/top/SaveRecovery.cpp
    Engine/src/top/PlayCombatRules.cpp
    Engine/src/top/HistoricalSkillExecution.cpp
    Engine/src/top/play/PlayCombat.cpp
    Engine/src/top/play/PlayEconomy.cpp
    Engine/src/top/play/PlayPresentation.cpp
    Engine/src/top/play/PlayProgression.cpp
    Engine/src/top/play/PlayWorld.cpp
    Engine/src/top/PlaySelfTest.cpp
    Engine/src/item/PlayItems.cpp
        Engine/src/mission/PlayMissions.cpp
    Engine/src/mission/PlayMissionJournal.cpp
        Engine/src/npc/PlayNpcInteraction.cpp
    Engine/src/gl/GfxDeviceGL.cpp
    Platform/common/GameMain.cpp
)

if(EMSCRIPTEN)
    include(cmake/Web.cmake)
    # Native audit tools and GLX tests below belong to the Linux build.
    return()
endif()

add_executable(top_gl ${TOP_GAME_SOURCES}
    Engine/src/top/Font.cpp
    Engine/src/gl/GlxWindow.cpp
    Platform/linux/main.cpp
)
target_include_directories(top_gl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
target_compile_definitions(top_gl PRIVATE TOP_USE_OPENGL)
find_package(Threads REQUIRED)
target_link_libraries(top_gl PRIVATE top_ui top_item GL X11 z ${CMAKE_DL_LIBS} Threads::Threads)

# 05-AF: inventario independente de contexto OpenGL, executavel tambem em CI.
add_executable(top_asset_audit
    Tools/asset_audit.cpp
    Engine/src/top/AssetAudit.cpp
    Engine/src/top/MapFile.cpp
)
target_include_directories(top_asset_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

install(TARGETS top_gl RUNTIME DESTINATION bin)

if(TOP_BUILD_UI_TESTS)
    add_executable(top_render_state_cache_gpu_tests tests/RenderStateCacheGpuTests.cpp
        Engine/src/gl/GfxDeviceGL.cpp Engine/src/gl/GlxWindow.cpp)
    target_include_directories(top_render_state_cache_gpu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_render_state_cache_gpu_tests PRIVATE GL X11 ${CMAKE_DL_LIBS})
    target_compile_options(top_render_state_cache_gpu_tests PRIVATE -UNDEBUG)
endif()

# Migracao 04-D: testes leves sem depender dos arquivos de dados do cliente.
# cmake -S . -B build-tests -DTOP_BUILD_UI_TESTS=ON
# cmake --build build-tests --target top_ui04d_tests && ctest --test-dir build-tests --output-on-failure
option(TOP_BUILD_UI_TESTS "Compile independent inventory/skills UI regression tests" OFF)
if(TOP_BUILD_UI_TESTS)
    enable_testing()
    add_executable(top_item_gem_voucher_tests tests/ItemGemVoucherRulesTests.cpp)
    target_link_libraries(top_item_gem_voucher_tests PRIVATE top_item)
    add_test(NAME item_gem_voucher_historical_tables COMMAND top_item_gem_voucher_tests)
    set_tests_properties(item_gem_voucher_historical_tables PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_executable(top_ui04d_tests
        tests/UI04DTests.cpp
        Engine/ui/UIManager.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
    )
    target_include_directories(top_ui04d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_compile_definitions(top_ui04d_tests PRIVATE TOP_USE_OPENGL)
    add_test(NAME ui04d_state_and_input COMMAND top_ui04d_tests)
endif()

# SK2.17-N: opt-in DX9 SetAlpha semantics for nested type-6 STRIP effects.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217n_strip_alpha_tests tests/SK217NStripAlphaTests.cpp)
    target_include_directories(top_ui05sk217n_strip_alpha_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217n_strip_parent_alpha COMMAND top_ui05sk217n_strip_alpha_tests)

    add_executable(top_ui05sk217n_strip_alpha_integration_tests tests/SK217NStripAlphaIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217n_strip_alpha_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217n_strip_alpha_integration_tests PRIVATE z)
    add_test(NAME ui05sk217n_strip_parent_alpha_integration COMMAND top_ui05sk217n_strip_alpha_integration_tests)
endif()

# SK2.17-O: discrete fixed-step acceleration integration for BLAST type 3.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217o_blast_motion_tests tests/SK217OBlastMotionTests.cpp)
    target_include_directories(top_ui05sk217o_blast_motion_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217o_blast_discrete_acceleration COMMAND top_ui05sk217o_blast_motion_tests)

    add_executable(top_ui05sk217o_blast_motion_integration_tests tests/SK217OBlastMotionIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217o_blast_motion_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217o_blast_motion_integration_tests PRIVATE z)
    add_test(NAME ui05sk217o_blast_discrete_acceleration_integration COMMAND top_ui05sk217o_blast_motion_integration_tests)
endif()

# SK2.17-P: source-derived SHRINK motion and its per-axis collision bounds.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217p_shrink_tests tests/SK217PShrinkTests.cpp)
    target_include_directories(top_ui05sk217p_shrink_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217p_historical_shrink COMMAND top_ui05sk217p_shrink_tests)

    add_executable(top_ui05sk217p_shrink_integration_tests tests/SK217PShrinkIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217p_shrink_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217p_shrink_integration_tests PRIVATE z)
    add_test(NAME ui05sk217p_historical_shrink_integration COMMAND top_ui05sk217p_shrink_integration_tests)
endif()

# SK2.17-Q: SNOW/FIRE shared emitter timer advances with actual step(dt).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217q_particle_timer_tests tests/SK217QParticleTimerTests.cpp)
    target_include_directories(top_ui05sk217q_particle_timer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217q_snow_fire_actual_dt COMMAND top_ui05sk217q_particle_timer_tests)

    add_executable(top_ui05sk217q_particle_timer_integration_tests tests/SK217QParticleTimerIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217q_particle_timer_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217q_particle_timer_integration_tests PRIVATE z)
    add_test(NAME ui05sk217q_snow_fire_actual_dt_integration COMMAND top_ui05sk217q_particle_timer_integration_tests)
endif()

# SK2.17-R: type-6 child EFF advances one frame per actual controller update.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217r_strip_eff_timeline_tests tests/SK217RStripEffTimelineTests.cpp)
    target_include_directories(top_ui05sk217r_strip_eff_timeline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217r_strip_eff_frame_step COMMAND top_ui05sk217r_strip_eff_timeline_tests)

    add_executable(top_ui05sk217r_strip_eff_timeline_integration_tests tests/SK217RStripEffTimelineIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217r_strip_eff_timeline_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217r_strip_eff_timeline_integration_tests PRIVATE z)
    add_test(NAME ui05sk217r_strip_eff_frame_step_integration COMMAND top_ui05sk217r_strip_eff_timeline_integration_tests)
endif()

# SK2.17-S: nested STRIP .eff controller-local UV and texture clocks.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217s_strip_eff_clock_tests tests/SK217SStripEffClockTests.cpp)
    target_include_directories(top_ui05sk217s_strip_eff_clock_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217s_strip_eff_controller_clocks COMMAND top_ui05sk217s_strip_eff_clock_tests)

    add_executable(top_ui05sk217s_strip_eff_clock_integration_tests tests/SK217SStripEffClockIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217s_strip_eff_clock_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217s_strip_eff_clock_integration_tests PRIVATE z)
    add_test(NAME ui05sk217s_strip_eff_controller_clocks_integration COMMAND top_ui05sk217s_strip_eff_clock_integration_tests)
endif()

# SK2.17-BP: shared nested .eff clock for SNOW/FIRE controllers.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217bp_snow_fire_eff_integration_tests tests/SK217BPSnowFireEffIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217bp_snow_fire_eff_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217bp_snow_fire_eff_integration_tests PRIVATE z)
    add_test(NAME ui05sk217bp_snow_fire_eff_shared_timeline COMMAND top_ui05sk217bp_snow_fire_eff_integration_tests)
endif()

# 04-E: testes offline de janelas modais, sem assets ou GL runtime.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui04e_tests
        tests/UI04ETests.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/UIManager.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
        Engine/ui/screens/NpcDialogUI.cpp
        Engine/ui/screens/ShopUI.cpp
        Engine/ui/screens/BoatUI.cpp
        Engine/ui/screens/SystemUI.cpp
        Engine/ui/screens/CharacterSelectUI.cpp
    )
    target_include_directories(top_ui04e_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui04e_modal_regression COMMAND top_ui04e_tests)
endif()

# 04-F: testes de comandos graficos gerados pelos modais sem recursos externos.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui04f_tests
        tests/UI04FTests.cpp
        Engine/ui/UIManager.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
        Engine/ui/screens/ShopUI.cpp
        Engine/ui/screens/NpcDialogUI.cpp
        Engine/ui/screens/BoatUI.cpp
        Engine/ui/screens/CharacterSelectUI.cpp
        Engine/ui/screens/SystemUI.cpp
        Engine/ui/screens/QuestUI.cpp
        Engine/ui/screens/CharacterStatusUI.cpp
        Engine/ui/screens/MinimapUI.cpp
    Engine/ui/screens/GameHUD.cpp
        Engine/ui/screens/ShortcutBarUI.cpp
    )
    target_include_directories(top_ui04f_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui04f_visual_commands COMMAND top_ui04f_tests)
endif()

# 04-G: regressao do Status/C, loja arrastavel e janelas procedurais.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui04g_tests
        tests/UI04GTests.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/UIManager.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
        Engine/ui/screens/ShopUI.cpp
        Engine/ui/screens/NpcDialogUI.cpp
        Engine/ui/screens/BoatUI.cpp
        Engine/ui/screens/SystemUI.cpp
        Engine/ui/screens/CharacterSelectUI.cpp
        Engine/ui/screens/CharacterStatusUI.cpp
        Engine/ui/screens/MinimapUI.cpp
    Engine/ui/screens/GameHUD.cpp
        Engine/ui/screens/QuestUI.cpp
        Engine/ui/screens/ShortcutBarUI.cpp
    )
    target_include_directories(top_ui04g_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui04g_procedural_regression COMMAND top_ui04g_tests)
endif()

# 04-H: composicao atomica por janela / sem textura, sem contexto GL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui04h_tests
        tests/UI04HTests.cpp
        Engine/ui/HudComposer.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/UIManager.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
        Engine/ui/screens/ShopUI.cpp
        Engine/ui/screens/NpcDialogUI.cpp
        Engine/ui/screens/BoatUI.cpp
        Engine/ui/screens/SystemUI.cpp
        Engine/ui/screens/CharacterSelectUI.cpp
        Engine/ui/screens/CharacterStatusUI.cpp
        Engine/src/top/Font.cpp
    )
    target_include_directories(top_ui04h_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui04h_tests PRIVATE ${CMAKE_DL_LIBS})
    add_test(NAME ui04h_layered_visibility COMMAND top_ui04h_tests)
endif()

# 04-I: independent minimap layout / hit-test / graphical command checks.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui04i_tests
        tests/UI04ITests.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/MinimapUI.cpp
    )
    target_include_directories(top_ui04i_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui04i_minimap_layout COMMAND top_ui04i_tests)
endif()

# 05-B: offline gameplay rules: stack capacities, equipment slot geometry, NPC Trade.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05b_tests
        tests/UI05BTests.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/NpcDialogUI.cpp
    )
    target_include_directories(top_ui05b_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05b_offline_gameplay COMMAND top_ui05b_tests)
endif()

# 05-C: 24->48 historical inventory expansion and scroll tests.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05c_tests
        tests/UI05CTests.cpp
        Engine/ui/UIManager.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
    )
    target_include_directories(top_ui05c_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05c_inventory_expansion COMMAND top_ui05c_tests)
endif()

# 05-D: regras historicas de selecao de animacao de armas, sem GL/assets.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05d_tests tests/UI05DTests.cpp)
    target_include_directories(top_ui05d_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05d_weapon_pose_modes COMMAND top_ui05d_tests)
endif()

# 05-E: ataques 7/8/9 e acoes de habilidades do campo Play Action do skillinfo.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05e_tests tests/UI05ETests.cpp)
    target_include_directories(top_ui05e_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05e_historical_action_poses COMMAND top_ui05e_tests)
endif()

# 05-F: historical effect IDs, projectile keyframes and impact bursts.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05f_tests
        tests/UI05FTests.cpp
        Engine/src/top/EffectSystem.cpp
        Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05f_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05f_tests PRIVATE z)
    add_test(NAME ui05f_effect_timeline COMMAND top_ui05f_tests)
endif()

# 05-G: data-driven historical effects (.eff references + .par controllers).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05g_tests
        tests/UI05GTests.cpp
        Engine/src/top/EffectSystem.cpp
        Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05g_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05g_tests PRIVATE z)
    add_test(NAME ui05g_historical_effect_resources COMMAND top_ui05g_tests)
    # Runs the synthetic effect/channel regression even in CI without Client/ assets.
    add_test(NAME ui05g_skill_channel_composition COMMAND top_ui05g_tests --fixture-only)
    add_test(NAME ui05sk24_magic_result_impact COMMAND top_ui05g_tests --fixture-only)
endif()

# 05-H: utilitario sem OpenGL para inspecionar cabecalhos e strings de .eff reais.
add_executable(top_eff_inspect Tools/eff_inspect.cpp)
target_include_directories(top_eff_inspect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

# 05-H: keyframes/rotacao do CMPPartCtrl, validacao binaria e fallback.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05h_tests
        tests/UI05HTests.cpp
        Engine/src/top/EffectSystem.cpp
        Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05h_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05h_tests PRIVATE z)
    add_test(NAME ui05h_particle_keyframes COMMAND top_ui05h_tests)
endif()

# 05-I: structured .eff (CMPResManger + I_Effect tracks) and GL billboard fallback.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05i_tests
        tests/UI05ITests.cpp
        Engine/src/top/EffectSystem.cpp
        Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05i_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05i_tests PRIVATE z)
    add_test(NAME ui05i_eff_keyframes_and_billboards COMMAND top_ui05i_tests)
endif()

# 05-J: animated UV coordinates, atlas regions and historical texture frames.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05j_tests
        tests/UI05JTests.cpp
        Engine/src/top/EffectSystem.cpp
        Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05j_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05j_tests PRIVATE z)
    add_test(NAME ui05j_uv_atlas_and_texture_frames COMMAND top_ui05j_tests)
endif()

# 05-K: original CEffectModel primitives with historical transforms and UVs.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05k_tests
        tests/UI05KTests.cpp
        Engine/src/top/EffectSystem.cpp
        Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05k_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05k_tests PRIVATE z)
    add_test(NAME ui05k_native_effect_primitives COMMAND top_ui05k_tests)
endif()

# 05-L: lwGeomObjInfo .lgo used by effect meshes (no GL context).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05l_tests
        tests/UI05LTests.cpp
        Engine/src/top/EffectSystem.cpp
        Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05l_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05l_tests PRIVATE z)
    add_test(NAME ui05l_external_effect_lgo COMMAND top_ui05l_tests)
endif()

# 05-M: regras offline de perseguicao, visao e retorno do ai.lua original.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05m_tests tests/UI05MTests.cpp)
    target_include_directories(top_ui05m_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05m_monster_ai_and_combat_range COMMAND top_ui05m_tests)
endif()

# 05-N: navegação A* local dos monstros, limitada pelo raio do spawn.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05n_tests tests/UI05NTests.cpp Engine/src/top/PathFinder.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05n_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05n_monster_pathfinding COMMAND top_ui05n_tests)
endif()

# 05-O: characterinfo Action ID vs actor ID; nonlooping death animations.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05o_tests tests/UI05OTests.cpp)
    target_include_directories(top_ui05o_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05o_monster_action_binding COMMAND top_ui05o_tests)
endif()

# 05-P: comportamento do ai_define.lua (AI 11 ataca-e-foge),
# memoria de alvo e passeio seguro dentro do raio de nascimento.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05p_tests tests/UI05PTests.cpp)
    target_include_directories(top_ui05p_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05p_monster_target_memory_and_retreat COMMAND top_ui05p_tests)
endif()

# 05-Q: characterinfo Skill ID/Rate + skillinfo alcance/visuais, sem GL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05q_tests tests/UI05QTests.cpp)
    target_include_directories(top_ui05q_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05q_monster_offensive_skills COMMAND top_ui05q_tests)
endif()

# 05-R: populacao historica por mapa, sem lista global de monstros iniciais.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05r_tests tests/UI05RTests.cpp Engine/src/top/MonsterSpawnSystem.cpp)
    target_include_directories(top_ui05r_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05r_historical_map_spawns COMMAND top_ui05r_tests)
endif()

# 05-S: registro literal de CreateCha em mlist.lua (sem executar scripts Lua).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05s_tests tests/UI05STests.cpp Engine/src/top/MonsterSpawnSystem.cpp)
    target_include_directories(top_ui05s_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05s_lua_scripted_map_spawns COMMAND top_ui05s_tests)
endif()

# 05-T: respawn historico CreateCha preservado no streaming/mapas offline.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05t_tests tests/UI05TTests.cpp Engine/src/top/MonsterSpawnSystem.cpp)
    target_include_directories(top_ui05t_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05t_persistent_lua_respawn COMMAND top_ui05t_tests)
endif()

# 05-U: atributos reais, DEXP, dano e esquiva; nao precisa de OpenGL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05u_tests tests/UI05UTests.cpp)
    target_include_directories(top_ui05u_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05u_historical_stats_experience COMMAND top_ui05u_tests)
endif()

# 05-U: inspetor independente dos atributos em characterinfo / DEXP.
add_executable(top_stats_inspect Tools/stats_inspect.cpp)
target_include_directories(top_stats_inspect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

# 05-V: resistencia, criticos e invariantes de combate historico.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05v_tests tests/UI05VTests.cpp)
    target_include_directories(top_ui05v_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05v_critical_resistance COMMAND top_ui05v_tests)
endif()

# Play::selfTest sem dependencias graficas (reutiliza as secoes de codigo
# necessarias ao gameplay; descarta apenas caminhos de HUD/ActorSkin nao usados).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05v_play_tests
        tests/UI05VPlayTests.cpp
        tests/PlaySelfTestActorSkinStubs.cpp
        Engine/src/top/Play.cpp
        Engine/src/top/SaveRecovery.cpp
        Engine/src/top/PlayCombatRules.cpp
        Engine/src/top/HistoricalSkillExecution.cpp
        Engine/src/top/play/PlayCombat.cpp
        Engine/src/top/play/PlayEconomy.cpp
        Engine/src/top/play/PlayPresentation.cpp
        Engine/src/top/play/PlayProgression.cpp
        Engine/src/top/play/PlayWorld.cpp
        Engine/src/top/PlaySelfTest.cpp
    Engine/src/item/PlayItems.cpp
        Engine/src/mission/MissionCatalog.cpp
        Engine/src/mission/PlayMissions.cpp
    Engine/src/mission/PlayMissionJournal.cpp
        Engine/src/npc/PlayNpcInteraction.cpp
        Engine/src/top/MapFile.cpp
        Engine/src/top/PathFinder.cpp)
    target_include_directories(top_ui05v_play_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_compile_options(top_ui05v_play_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(top_ui05v_play_tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(top_ui05v_play_tests PRIVATE z top_item)
    add_test(NAME ui05v_play_state_regression COMMAND top_ui05v_play_tests)
    add_test(NAME sk220_pet_equipment_skill_stats COMMAND top_ui05v_play_tests --pet-stats-only)
    set_tests_properties(sk220_pet_equipment_skill_stats PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
    # 05-AF: ao usar a estrutura sources/ + Client/ lado a lado, o CTest
    # encontra os scripts sem exigir copia manual ou symlink em build/.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Client/scripts/table/skillinfo.txt" AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Client/scripts/offline/calculate/variable.lua")
        get_filename_component(TOP_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
        set_tests_properties(ui05v_play_state_regression PROPERTIES
            WORKING_DIRECTORY "${TOP_RUNTIME_ROOT}")
    endif()
endif()

# 05-BI: input/target regression independent of historical Client assets.
if(TOP_BUILD_UI_TESTS)
    get_target_property(TOP_PLAY_SOURCES top_ui05v_play_tests SOURCES)
    list(REMOVE_ITEM TOP_PLAY_SOURCES tests/UI05VPlayTests.cpp)
    add_executable(top_ui05bi_tests tests/UI05BITests.cpp ${TOP_PLAY_SOURCES})
    target_include_directories(top_ui05bi_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_compile_options(top_ui05bi_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(top_ui05bi_tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(top_ui05bi_tests PRIVATE z top_item)
    add_test(NAME ui05bi_explicit_skill_target COMMAND top_ui05bi_tests)
endif()

# 05-W: colunas de bonus estaticos iteminfo.txt (atributos, HP/SP, HIT/FLEE, CRT/PDEF).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05w_tests tests/UI05WTests.cpp)
    target_include_directories(top_ui05w_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05w_equipment_primary_and_derived_stats COMMAND top_ui05w_tests)
endif()

# 05-W1: selecao 3D (cuerpo de monstros, NPCs e loot) sem OpenGL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05w1_tests tests/UI05W1Tests.cpp)
    target_include_directories(top_ui05w1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05w1_screen_ray_selection COMMAND top_ui05w1_tests)
endif()

# 05-W2: camera elevada, + de habilidade, modal de venda/descarte.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05w2_tests
        tests/UI05W2Tests.cpp
        Engine/ui/screens/SkillsUI.cpp
        Engine/ui/UIRenderer.cpp)
    target_include_directories(top_ui05w2_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05w2_classic_camera_drag_and_confirm COMMAND top_ui05w2_tests)
endif()

# 05-X: ASPD do ItemInfo, selecao azul, zoom e medidor headless.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05x_tests tests/UI05XTests.cpp)
    target_include_directories(top_ui05x_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05x_attack_speed_camera_selection_performance COMMAND top_ui05x_tests)
endif()

# 05-X1: regressao do streaming de terreno (independente da animacao de cena).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05x1_tests tests/UI05X1Tests.cpp)
    target_include_directories(top_ui05x1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05x1_terrain_streaming_regression COMMAND top_ui05x1_tests)
endif()

# 05-X2: teste headless para ciclo de vida dos buffers do terreno/cenario.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05x2_tests tests/UI05X2Tests.cpp)
    target_include_directories(top_ui05x2_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05x2_gpu_cache_and_terrain_lifecycle COMMAND top_ui05x2_tests)
endif()

# 05-X3: cenario estatico fica no VBO; apenas os objetos animados mudam a 15 Hz.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05x3_tests
        tests/UI05X3Tests.cpp
        Engine/src/top/SceneWorld.cpp
        Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp
        Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05x3_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05x3_static_animated_scene_equivalence COMMAND top_ui05x3_tests)
endif()

# 05-X4: evita deformar o mesmo vertice varias vezes em indices/subsets,
# mas continua atualizando yaw/posicao do ator em todos os quadros.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05x4_tests tests/UI05X4Tests.cpp)
    target_include_directories(top_ui05x4_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05x4_actor_skinning_vertex_cache COMMAND top_ui05x4_tests)
endif()

# 05-X5: reusa VBOs de personagens se os vertices sao exatamente identicos.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05x5_tests tests/UI05X5Tests.cpp)
    target_include_directories(top_ui05x5_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05x5_actor_gpu_batch_cache COMMAND top_ui05x5_tests)
endif()

# 05-X6: teste matematico headless do frustum usado pelo ActorSkin.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05x6_tests tests/UI05X6Tests.cpp)
    target_include_directories(top_ui05x6_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05x6_actor_frustum_and_clip_planes COMMAND top_ui05x6_tests)
endif()

# 05-Y: persistencia de instancias fisicas de itens e atributos individuais.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05y_tests tests/UI05YTests.cpp)
    target_include_directories(top_ui05y_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05y_item_instance_persistence COMMAND top_ui05y_tests)
endif()

# 05-Z: gemas declarativas do variable.lua, custo/risco do forge.lua, persistencia Z1.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05z_tests tests/UI05ZTests.cpp)
    target_include_directories(top_ui05z_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05z_gem_forge_rules_and_persistence COMMAND top_ui05z_tests)
endif()

# 05-AA: perfuracao historica, custo progressivo, materiais 890+891 e apparel.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05aa_tests tests/UI05AATests.cpp)
    target_include_directories(top_ui05aa_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05aa_historical_socket_fusion COMMAND top_ui05aa_tests)
endif()

# 05-AB: item tooltip inspection and same-ID equipment instance regression.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ab_tests
        tests/UI05ABTests.cpp
        Engine/ui/UIManager.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
    )
    target_include_directories(top_ui05ab_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ab_inventory_item_tooltip_and_uid COMMAND top_ui05ab_tests)
endif()

# 05-AC: bancada visual, selecao por UID e preview de custo/chance.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ac_tests
        tests/UI05ACTests.cpp
        Engine/ui/screens/ForgeWorkbenchUI.cpp
        Engine/ui/UIManager.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
    )
    target_include_directories(top_ui05ac_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ac_forge_workbench_uid_and_preview COMMAND top_ui05ac_tests)
endif()

# 05-AD: leitura segura de servicos Lua de NPC, botoes Forge e bancada restrita.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ad_tests
        tests/UI05ADTests.cpp
        Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp
        Engine/ui/screens/NpcDialogUI.cpp
        Engine/ui/screens/ForgeWorkbenchUI.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/InventoryUI.cpp
        Engine/ui/screens/SkillsUI.cpp
    )
    target_include_directories(top_ui05ad_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ad_historical_npc_forge_service_binding COMMAND top_ui05ad_tests)
endif()

# 05-AE: parser estrito de opcoes Text() em funcoes Lua dos NPCs.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ae_tests tests/UI05AETests.cpp)
    target_include_directories(top_ui05ae_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ae_npc_lua_tokenizer_and_forge_service_isolation COMMAND top_ui05ae_tests)
endif()

# 05-AF: cobertura de dados, caixa Linux, .map e relatorios JSON/CSV.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05af_tests
        tests/UI05AFTests.cpp
        Engine/src/top/AssetAudit.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05af_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05af_assets_diagnostics COMMAND top_ui05af_tests)
endif()

# 05-AG: resolver nomes Windows sem renomear assets no Linux.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ag_tests
        tests/UI05AGTests.cpp
        Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp
        Engine/src/top/PortalSystem.cpp
        Engine/src/top/MonsterSpawnSystem.cpp
    )
    target_include_directories(top_ui05ag_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ag_historical_case_paths COMMAND top_ui05ag_tests)
endif()

# 05-AH: cruza instancias .obj com IDs de sceneobjinfo para separar
# modelos de cena realmente usados dos historicos apenas catalogados.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ah_tests
        tests/UI05AHTests.cpp
        Engine/src/top/AssetAudit.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05ah_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ah_scene_usage_vs_catalog COMMAND top_ui05ah_tests)
endif()

# 05-AI: ranking dos mapas afetados e lista dos modelos historicos faltantes.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ai_tests
        tests/UI05AITests.cpp
        Engine/src/top/AssetAudit.cpp
        Engine/src/top/MapFile.cpp
    )
    target_include_directories(top_ui05ai_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ai_scene_missing_impact COMMAND top_ui05ai_tests)
endif()

# 05-AJ: parser passivo de menus NPC e escolha de missoes elegiveis.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05aj_tests
        tests/UI05AJTests.cpp
        Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp
    )
    target_include_directories(top_ui05aj_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05aj_npc_menus_and_eligible_missions COMMAND top_ui05aj_tests)
endif()

# 05-AK: painel de missoes multiopcao em UI procedural + diario.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ak_tests
        tests/UI05AKTests.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/NpcDialogUI.cpp
        Engine/ui/screens/QuestUI.cpp
    )
    target_include_directories(top_ui05ak_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ak_classic_quest_ui COMMAND top_ui05ak_tests)
endif()

# 05-AK.1: indice de missoes evita releitura de todos os arquivos Lua na
# abertura da janela. Teste totalmente headless com scripts artificiais.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ak1_tests
        tests/UI05AK1Tests.cpp
        Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp
    )
    target_include_directories(top_ui05ak1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ak1_cached_mission_index_and_reload COMMAND top_ui05ak1_tests)
endif()

# 05-AK.2: diagnostico de inicializacao ate o primeiro swap, sem GL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ak2_tests tests/UI05AK2Tests.cpp)
    target_include_directories(top_ui05ak2_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ak2_startup_timing_csv COMMAND top_ui05ak2_tests)
endif()

# 05-AK.3: catalogo Lua de NPCs indexado uma unica vez por mapa.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ak3_tests
        tests/UI05AK3Tests.cpp
        Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp
    )
    target_include_directories(top_ui05ak3_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ak3_single_pass_npc_lua_catalog COMMAND top_ui05ak3_tests)
endif()

# 05-AL: diario paginado e fluxo de missoes historicas simultaneas.
# O Play::selfTest exercita ainda save Q2, hunts compartilhadas e abandono.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05al_tests
        tests/UI05ALTests.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/QuestUI.cpp
    )
    target_include_directories(top_ui05al_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05al_parallel_quest_diary COMMAND top_ui05al_tests)
endif()

# 05-AM: regras de elegibilidade passivas dos scripts historicos.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05am_tests tests/UI05AMTests.cpp Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp)
    target_include_directories(top_ui05am_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05am_mission_prerequisites_and_job COMMAND top_ui05am_tests)
endif()

# 05-AO: descoberta de missoes no mapa e coordenadas do painel OpenGL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ao_tests tests/UI05AOTests.cpp)
    target_include_directories(top_ui05ao_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05ao_map_quest_discovery_layout COMMAND top_ui05ao_tests)
endif()

# 05-AP: entrega comprovada pelos MissionScript Lua; headless, requer
# os scripts historicos na pasta Client/ (ZIP scriptsclientatual).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ap_tests tests/UI05APTests.cpp Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp)
    target_include_directories(top_ui05ap_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Client/scripts/offline/script/MisScript/MissionScript01.lua")
        add_test(NAME ui05ap_historical_npc_turn_in COMMAND top_ui05ap_tests)
    endif()
endif()

# 05-AQ: flags limitadas aos registros de missao; os testes historicos
# exigem scripts do Client, mas nao bibliotecas OpenGL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05aq_tests tests/UI05AQTests.cpp Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp)
    target_include_directories(top_ui05aq_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Client/scripts/offline/script/MisScript/MissionScript01.lua")
        add_test(NAME ui05aq_mission_scoped_flags COMMAND top_ui05aq_tests)
    endif()
endif()

# 05-AR: handoff between a specific side-result NPC and the offering NPC.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ar_tests tests/UI05ARTests.cpp Engine/src/npc/NpcSystem.cpp
        Engine/src/mission/MissionCatalog.cpp)
    target_include_directories(top_ui05ar_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Client/scripts/offline/script/MisScript/MissionScript01.lua")
        add_test(NAME ui05ar_historical_cross_npc_step COMMAND top_ui05ar_tests)
    endif()
endif()

# 05-AS: render procedural for explicit Accept / Complete mission details.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05as_tests
        tests/UI05ASTests.cpp Engine/ui/screens/NpcDialogUI.cpp Engine/ui/UIRenderer.cpp)
    target_include_directories(top_ui05as_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05as_explicit_quest_completion_ui COMMAND top_ui05as_tests)
endif()

# 05-AT: prova que hit-test de NPC e renderer da UI nao precisam de
# ActorSkin, Play, OpenGL nem da biblioteca de personagens.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05at_tests
        tests/UI05ATTests.cpp
        Engine/ui/UIRenderer.cpp
        Engine/ui/screens/NpcDialogUI.cpp
    )
    target_include_directories(top_ui05at_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Engine/ui
    )
    add_test(NAME ui05at_decoupled_quest_dialog COMMAND top_ui05at_tests)
endif()

# Compila todas as regressoes headless sem obrigar a vinculacao do top_gl.
# Util onde o SDK de desenvolvimento OpenGL ou os assets Client faltam.
if(TOP_BUILD_UI_TESTS)
    get_property(top_all_targets DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    set(top_test_targets)
    foreach(top_target IN LISTS top_all_targets)
        if(top_target MATCHES "^top_ui.*_tests$")
            list(APPEND top_test_targets ${top_target})
        endif()
    endforeach()
    add_custom_target(top_regression_tests DEPENDS ${top_test_targets})
endif()

if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05at_domains_tests tests/UI05ATDomainsTests.cpp Engine/src/mission/MissionCatalog.cpp)
    target_include_directories(top_ui05at_domains_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05at_independent_mission_catalog COMMAND top_ui05at_domains_tests)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Client/scripts/offline/script/MisScript/MissionScript01.lua")
        set_tests_properties(ui05at_independent_mission_catalog PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
    endif()
    add_dependencies(top_regression_tests top_ui05at_domains_tests)
endif()

# Work 2026-10-23: registra o teste de paridade do mar na suite headless.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_sea_terrain_parity_tests
        tests/SeaTerrainParityTests.cpp
        Engine/src/top/MapFile.cpp
        Engine/src/top/TerrainMesh.cpp
    )
    target_include_directories(top_sea_terrain_parity_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include
    )
    add_test(NAME sea_terrain_parity COMMAND top_sea_terrain_parity_tests)
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_sea_terrain_parity_tests)
    endif()
endif()

# 05-AU: parser historical TE_KILL/TE_GETITEM/AddNextFlag, independent
# counters and flag completion without OpenGL or NPC implementation.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05au_tests tests/UI05AUTests.cpp Engine/src/mission/MissionCatalog.cpp)
    target_include_directories(top_ui05au_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05au_historical_event_triggers COMMAND top_ui05au_tests)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Client/scripts/offline/script/MisScript/MissionScript01.lua")
        set_tests_properties(ui05au_historical_event_triggers PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
    endif()
    add_dependencies(top_regression_tests top_ui05au_tests)
endif()

# 05-AV: diario e progresso por gatilho testados sem UI ou OpenGL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05av_tests tests/UI05AVTests.cpp)
    target_include_directories(top_ui05av_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05av_modular_mission_journal COMMAND top_ui05av_tests)
    add_dependencies(top_regression_tests top_ui05av_tests)
endif()

# 05-AW: regressao da politica de uso/slots e contrato de item sem OpenGL.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05aw_tests tests/UI05AWTests.cpp)
    target_link_libraries(top_ui05aw_tests PRIVATE top_item)
    add_test(NAME ui05aw_modular_item_use_rules COMMAND top_ui05aw_tests)
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05aw_tests)
    endif()
endif()

# 05-AX: baus deterministicos historicos isolados do renderer e do Play.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ax_tests tests/UI05AXTests.cpp)
    target_link_libraries(top_ui05ax_tests PRIVATE top_item)
    add_test(NAME ui05ax_historical_deterministic_chests COMMAND top_ui05ax_tests)
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05ax_tests)
    endif()
endif()

# 05-AY: ticket historical destination mapping and conditional armor boxes.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ay_tests tests/UI05AYTests.cpp)
    target_link_libraries(top_ui05ay_tests PRIVATE top_item)
    add_test(NAME ui05ay_historical_tickets_and_armor_chests COMMAND top_ui05ay_tests)
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05ay_tests)
    endif()
endif()

# 05-AZ: Newbie Chests historicos, regras puras e sem dependencias graficas.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05az_tests tests/UI05AZTests.cpp)
    target_link_libraries(top_ui05az_tests PRIVATE top_item)
    add_test(NAME ui05az_newbie_chests_and_restrictions COMMAND top_ui05az_tests)
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05az_tests)
    endif()
endif()

# 05-BA: contrato puro da inicializacao historica de instancias de item.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05ba_tests tests/UI05BATests.cpp)
    target_link_libraries(top_ui05ba_tests PRIVATE top_item)
    add_test(NAME ui05ba_item_instantiation_contract COMMAND top_ui05ba_tests)
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05ba_tests)
    endif()
endif()

# 05-BC: callback GiveRandomItems e tabelas de pesos históricos do variable.lua.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05bc_tests tests/UI05BCTests.cpp)
    target_link_libraries(top_ui05bc_tests PRIVATE top_item)
    add_test(NAME ui05bc_historical_random_item_rewards COMMAND top_ui05bc_tests)
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05bc_tests)
    endif()
endif()

# 05-BD: regras históricas dos livros de redistribuição de atributos.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05bd_tests tests/UI05BDTests.cpp)
    target_link_libraries(top_ui05bd_tests PRIVATE top_item)
    add_test(NAME ui05bd_historical_item_stat_resets COMMAND top_ui05bd_tests)
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05bd_tests)
    endif()
endif()

# 05-BE: catálogo e faixas de peso dos blueprints históricos de produção.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05be_tests tests/UI05BETests.cpp)
    target_link_libraries(top_ui05be_tests PRIVATE top_item)
    add_test(NAME ui05be_historical_production_recipes COMMAND top_ui05be_tests)
    set_tests_properties(ui05be_historical_production_recipes PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05be_tests)
    endif()
endif()

# 05-BB: catalogo local de personagens, nascimento historico e save isolado.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05bb_tests tests/UI05BBTests.cpp)
    target_include_directories(top_ui05bb_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05bb_offline_character_catalog COMMAND top_ui05bb_tests)
    set_tests_properties(ui05bb_offline_character_catalog PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
    if(TARGET top_regression_tests)
        add_dependencies(top_regression_tests top_ui05bb_tests)
    endif()
endif()

# 05-BL: generic target/range/one-shot historical execution regression.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk1_tests tests/UI05SK1Tests.cpp ${TOP_PLAY_SOURCES})
    target_include_directories(top_ui05sk1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_compile_options(top_ui05sk1_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(top_ui05sk1_tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(top_ui05sk1_tests PRIVATE z top_item)
    add_test(NAME ui05sk1_historical_skill_execution COMMAND top_ui05sk1_tests)
endif()

# 05-BL: report historical skill execution/visual metadata coverage.
add_executable(top_skill_execution_audit
    Tools/skill_execution_audit.cpp
    Engine/src/top/HistoricalSkillExecution.cpp
)
target_include_directories(top_skill_execution_audit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

# SK2.3: standalone, read-only audit of all historical particle controllers.
# Must compile without GLX/OpenGL or original Client/ assets.
add_executable(top_par_audit Tools/par_audit.cpp)
target_include_directories(top_par_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk23_par_tests tests/SK23ParAuditTests.cpp)
    target_include_directories(top_sk23_par_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk23_all_par_audit COMMAND top_sk23_par_tests)
endif()

# Blend audit (SK2.5): read-only inventory of authored D3D blend state.
add_executable(top_blend_audit Tools/blend_audit.cpp)
target_include_directories(top_blend_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
if(TOP_BUILD_UI_TESTS)
    add_executable(top_blend_audit_tests tests/BlendAuditTests.cpp)
    target_include_directories(top_blend_audit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05blend_audit COMMAND top_blend_audit_tests)
endif()

# SK2.6: test-only DX9 blend-factor translator, independent of GL headers.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk26_tests tests/UI05SK26Tests.cpp)
    target_include_directories(top_ui05sk26_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk26_historical_blend_factors COMMAND top_ui05sk26_tests)
endif()

# SK2.7: DX9 I_Effect per-key duration timeline (CPU-only regression).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk27_tests tests/UI05SK27Tests.cpp)
    target_include_directories(top_ui05sk27_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk27_eff_duration_keyframes COMMAND top_ui05sk27_tests)
endif()

# SK2.8 — DX9 Part_fshade historical flight drawing, CPU-only tests.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk28_tests tests/UI05SK28Tests.cpp)
    target_include_directories(top_ui05sk28_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk28_magic_fshade_motion COMMAND top_ui05sk28_tests)
endif()

# SK2.9: historical paths and runtime integration, no graphics context needed.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk29_tests tests/UI05SK29Tests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk29_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk29_tests PRIVATE z)
    add_test(NAME ui05sk29_historical_magic_paths COMMAND top_ui05sk29_tests)
endif()

# SK2.10: particle curves, native geometry and nested controller lifetime.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk210_tests tests/UI05SK210Tests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk210_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk210_tests PRIVATE z)
    add_test(NAME ui05sk210_particle_curves_models COMMAND top_ui05sk210_tests)
endif()

# Execute all table-declared effect channels without combat, saves or a GL context.
add_executable(top_skill_fx_probe Tools/skill_fx_probe.cpp
    Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
    Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
target_include_directories(top_skill_fx_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
target_link_libraries(top_skill_fx_probe PRIVATE z)

# SK2.11: missing nested geometry/black masks and hand attachment during skills.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk219_attachment_units_tests tests/SK219AttachmentUnitsTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk219_attachment_units_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk219_attachment_units_tests PRIVATE z)
    add_test(NAME sk219_attachment_world_units COMMAND top_sk219_attachment_units_tests)
    add_executable(top_ui05sk212_tests tests/UI05SK212Tests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk212_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk212_tests PRIVATE z)
    add_test(NAME ui05sk212_camera_and_nested_transforms COMMAND top_ui05sk212_tests)

    add_executable(top_ui05sk211_tests tests/UI05SK211Tests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk211_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk211_tests PRIVATE z)
    add_test(NAME ui05sk211_nested_effect_geometry COMMAND top_ui05sk211_tests)

    add_executable(top_actor_skill_pose_tests tests/ActorSkillPoseTests.cpp
        Engine/ui/ActorSkinUIBridge.cpp Engine/src/top/ActorSkin.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/SceneWorld.cpp Engine/src/top/Image.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_actor_skill_pose_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_compile_options(top_actor_skill_pose_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(top_actor_skill_pose_tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(top_actor_skill_pose_tests PRIVATE top_ui z Threads::Threads)
    add_test(NAME ui05sk211_actor_skill_hand_dummies COMMAND top_actor_skill_pose_tests)

    # Run explicitly under Xvfb where a graphics context is available. It is
    # intentionally not required by headless CTest environments.
    add_executable(top_effect_blend_gpu_tests tests/EffectBlendGpuTests.cpp
        Engine/src/gl/GfxDeviceGL.cpp Engine/src/gl/GlxWindow.cpp)
    target_include_directories(top_effect_blend_gpu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_effect_blend_gpu_tests PRIVATE GL X11 ${CMAKE_DL_LIBS})
endif()

# SK2.14: CPU-only coverage audit comparing authored .eff/.par data with
# source-code branches; no OpenGL, external Client assets or Python required.
add_executable(top_fx_coverage_audit Tools/fx_coverage_audit.cpp)
target_include_directories(top_fx_coverage_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

# SK2.17-T: map nested STRIP .eff controller types to Client usage and DX9 code.
add_executable(top_strip_eff_animation_audit Tools/strip_eff_animation_audit.cpp)
target_include_directories(top_strip_eff_animation_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

# Read-only comparison of type-6 parent alpha against nested .eff key alphas.
add_executable(top_strip_alpha_audit Tools/strip_alpha_audit.cpp)
target_include_directories(top_strip_alpha_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

# Read-only direct CMPStrip audit: decode each .par tail and compare DX9/GL owner paths.
add_executable(top_direct_strip_anchor_audit Tools/direct_strip_anchor_audit.cpp)
target_include_directories(top_direct_strip_anchor_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)

# SK2.17-AY: read-only inventory of static scene-effect placements in map .obj files.
add_executable(top_scene_effect_map_audit Tools/scene_effect_map_audit.cpp)
target_include_directories(top_scene_effect_map_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
if(TOP_BUILD_UI_TESTS)
    add_executable(top_fx_coverage_tests tests/FxCoverageAuditTests.cpp)
    target_include_directories(top_fx_coverage_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk214_fx_coverage_audit COMMAND top_fx_coverage_tests)
    set_tests_properties(ui05sk214_fx_coverage_audit PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

    add_executable(top_strip_eff_animation_audit_tests tests/StripEffAnimationAuditTests.cpp)
    target_include_directories(top_strip_eff_animation_audit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217t_strip_eff_animation_audit COMMAND top_strip_eff_animation_audit_tests)
    set_tests_properties(ui05sk217t_strip_eff_animation_audit PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

    add_executable(top_direct_strip_anchor_audit_tests tests/DirectStripAnchorAuditTests.cpp)
    target_include_directories(top_direct_strip_anchor_audit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217v_direct_strip_anchor_audit COMMAND top_direct_strip_anchor_audit_tests)

    add_executable(top_scene_effect_map_audit_tests tests/SceneEffectMapAuditTests.cpp)
    target_include_directories(top_scene_effect_map_audit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217ay_scene_effect_map_audit COMMAND top_scene_effect_map_audit_tests)

    add_executable(top_sk217w_model_timeline_tests tests/SK217WModelTimelineTests.cpp)
    target_include_directories(top_sk217w_model_timeline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217w_model_frame_timeline COMMAND top_sk217w_model_timeline_tests)

    add_executable(top_sk217am_round_timeline_tests tests/SK217AMRoundTimelineTests.cpp)
    target_include_directories(top_sk217am_round_timeline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217am_round_frame_timeline COMMAND top_sk217am_round_timeline_tests)

    add_executable(top_sk217am_round_integration_tests tests/SK217AMRoundIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217am_round_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217am_round_integration_tests PRIVATE z)
    add_test(NAME ui05sk217am_round_renderer_timeline COMMAND top_sk217am_round_integration_tests)

    add_executable(top_sk217aj_model_eff_timeline_tests tests/SK217AJModelEffTimelineTests.cpp)
    target_include_directories(top_sk217aj_model_eff_timeline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217aj_model_nested_eff_frame_group COMMAND top_sk217aj_model_eff_timeline_tests)

    add_executable(top_sk217aj_model_eff_integration_tests tests/SK217AJModelEffIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217aj_model_eff_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217aj_model_eff_integration_tests PRIVATE z)
    add_test(NAME ui05sk217aj_model_nested_eff_timeline COMMAND top_sk217aj_model_eff_integration_tests)

    add_executable(top_sk217bs_model_lifetime_tests tests/SK217BSModelLifetimeTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217bs_model_lifetime_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217bs_model_lifetime_tests PRIVATE z)
    add_test(NAME ui05sk217bs_model_controller_lifetime COMMAND top_sk217bs_model_lifetime_tests)
    set_tests_properties(ui05sk217bs_model_controller_lifetime PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

    add_executable(top_sk217x_ripple_timeline_tests tests/SK217XRippleTimelineTests.cpp)
    target_include_directories(top_sk217x_ripple_timeline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217x_ripple_emission_recycling COMMAND top_sk217x_ripple_timeline_tests)

    add_executable(top_sk217x_ripple_integration_tests tests/SK217XRippleIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217x_ripple_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217x_ripple_integration_tests PRIVATE z)
    add_test(NAME ui05sk217x_ripple_renderer_gate COMMAND top_sk217x_ripple_integration_tests)

    add_executable(top_sk217y_ripple_eff_integration_tests tests/SK217YRippleEffIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217y_ripple_eff_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217y_ripple_eff_integration_tests PRIVATE z)
    add_test(NAME ui05sk217y_ripple_eff_timeline COMMAND top_sk217y_ripple_eff_integration_tests)

    add_executable(top_sk217ab_arraw_eff_integration_tests tests/SK217ABArrawEffIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217ab_arraw_eff_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217ab_arraw_eff_integration_tests PRIVATE z)
    add_test(NAME ui05sk217ab_arraw_eff_timeline COMMAND top_sk217ab_arraw_eff_integration_tests)
endif()

# SK2.15-A: historical direct CMPStrip controllers, item dummy endpoints and
# texture/blend-preserving ribbon geometry (CPU-only integration).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk215_tests tests/SK215DirectStripTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk215_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk215_tests PRIVATE z)
    add_test(NAME ui05sk215_direct_controller_cmpstrip COMMAND top_ui05sk215_tests)
endif()

# SK2.15-B: conventional CMPPartSys STRIP nested-.eff group cycle,
# separate from direct CMPStrip between live item dummies.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk215b_tests tests/SK215BStripCycleTests.cpp)
    target_include_directories(top_ui05sk215b_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk215b_conventional_strip_cycles COMMAND top_ui05sk215b_tests)
endif()

# SK2.15-C: same direct-strip/particle integration fixture, second run with
# DX9 POINT/LINEAR filters enabled and explicit batch-isolation assertions.
if(TOP_BUILD_UI_TESTS)
    add_test(NAME ui05sk215c_authored_particle_filters COMMAND top_ui05sk215_tests)
    set_tests_properties(ui05sk215c_authored_particle_filters PROPERTIES
        ENVIRONMENT "TOP_HISTORICAL_FILTERS=1")
    add_executable(top_ui05sk215c_filter_tests tests/SK215CTextureFilterTests.cpp)
    target_include_directories(top_ui05sk215c_filter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk215c_filter_policy COMMAND top_ui05sk215c_filter_tests)
    # GL framebuffer test intentionally manual (Xvfb/desktop required).
    add_executable(top_effect_filter_gpu_tests tests/EffectTextureFilterGpuTests.cpp
        Engine/src/gl/GfxDeviceGL.cpp Engine/src/gl/GlxWindow.cpp)
    target_include_directories(top_effect_filter_gpu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_effect_filter_gpu_tests PRIVATE GL X11 ${CMAKE_DL_LIBS})
endif()

# SK2.16-A: pure C++ regression for DX9-like FIRE/SNOW/BLAST lifecycle.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk216a_tests tests/SK216AParticlePoolTests.cpp)
    target_include_directories(top_ui05sk216a_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk216a_particle_pool_lifecycle COMMAND top_ui05sk216a_tests)
endif()

if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk216a_integration_tests tests/SK216AParticleIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk216a_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk216a_integration_tests PRIVATE z)
    add_test(NAME ui05sk216a_particle_pool_integration COMMAND top_ui05sk216a_integration_tests)
endif()

# SK2.16-B: opt-in DX9 particle velocity/range coordinate policy.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk216b_tests tests/SK216BParticleMotionTests.cpp)
    target_include_directories(top_ui05sk216b_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk216b_particle_motion COMMAND top_ui05sk216b_tests)

    add_executable(top_ui05sk216b_integration_tests tests/SK216BParticleMotionIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk216b_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk216b_integration_tests PRIVATE z)
    add_test(NAME ui05sk216b_particle_motion_integration COMMAND top_ui05sk216b_integration_tests)
endif()

# SK2.16-D: effect texture precedence / mixed-case PNG compatibility (headless).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk216d_texture_tests tests/SK216DEffectTextureTests.cpp
        Engine/src/top/TexturePaths.cpp)
    target_include_directories(top_ui05sk216d_texture_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk216d_effect_texture_precedence COMMAND top_ui05sk216d_texture_tests)
endif()

# SK2.17-A: historically serialized _iRoadom affects opt-in particle life/motion.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217a_random_tests tests/SK217AParticleRandomTests.cpp)
    target_include_directories(top_ui05sk217a_random_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217a_particle_random_divisor COMMAND top_ui05sk217a_random_tests)
endif()

# SK2.17-B: deterministic extended historical particle trajectories (DX9 Z-up).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217b_extended_tests tests/SK217BExtendedParticleTests.cpp)
    target_include_directories(top_ui05sk217b_extended_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217b_extended_particle_modes COMMAND top_ui05sk217b_extended_tests)
endif()

# SK2.17-M: opt-in terrain projection mesh for the original SHADE controller.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217m_shade_tests tests/SK217MShadeMeshTests.cpp)
    target_include_directories(top_ui05sk217m_shade_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217m_historical_shade_mesh COMMAND top_ui05sk217m_shade_tests)

    add_executable(top_ui05sk217m_shade_integration_tests tests/SK217MShadeIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217m_shade_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217m_shade_integration_tests PRIVATE z)
    add_test(NAME ui05sk217m_historical_shade_integration COMMAND top_ui05sk217m_shade_integration_tests)
endif()

# SK2.17-B: verify historical positions and emissions reach EffectSystem batches.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217b_integration_tests tests/SK217BParticleIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217b_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217b_integration_tests PRIVATE z)
    add_test(NAME ui05sk217b_extended_particle_integration COMMAND top_ui05sk217b_integration_tests)
endif()

# SK2.17-C: DX9 CMPPartSys BLAST2 startup and lateral state machine.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217c_blast2_tests tests/SK217CBlast2Tests.cpp)
    target_include_directories(top_ui05sk217c_blast2_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217c_blast2_states COMMAND top_ui05sk217c_blast2_tests)

    add_executable(top_ui05sk217c_integration_tests tests/SK217CBlast2IntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217c_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217c_integration_tests PRIVATE z)
    add_test(NAME ui05sk217c_blast2_integration COMMAND top_ui05sk217c_integration_tests)
endif()

# SK2.17-D: opt-in RANGE2 terrain/bounds collision and serialized hitEffect.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_ui05sk217d_range2_tests tests/SK217DRange2CollisionTests.cpp)
    target_include_directories(top_ui05sk217d_range2_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217d_range2_collision COMMAND top_ui05sk217d_range2_tests)

    add_executable(top_sk217bi_range2_randf_tests tests/SK217BIRange2RandfTests.cpp)
    target_include_directories(top_sk217bi_range2_randf_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME sk217bi_range2_dx9_randf_mapping COMMAND top_sk217bi_range2_randf_tests)

    add_executable(top_ui05sk217d_range2_integration_tests tests/SK217DRange2IntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_ui05sk217d_range2_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_ui05sk217d_range2_integration_tests PRIVATE z)
    add_test(NAME ui05sk217d_range2_hit_effect_integration COMMAND top_ui05sk217d_range2_integration_tests)

    add_executable(top_ui05sk217e_range2_recycle_tests tests/SK217ERange2RecycleTests.cpp)
    target_include_directories(top_ui05sk217e_range2_recycle_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217e_range2_slot_recycling COMMAND top_ui05sk217e_range2_recycle_tests)

    add_executable(top_ui05sk217h_fire_hit_tests tests/SK217HFireHitTests.cpp)
    target_include_directories(top_ui05sk217h_fire_hit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217h_fire_path_hit_event COMMAND top_ui05sk217h_fire_hit_tests)

    add_executable(top_ui05sk217i_particle_count_tests tests/SK217IParticleCountTests.cpp)
    target_include_directories(top_ui05sk217i_particle_count_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217i_authored_snow_fire_count COMMAND top_ui05sk217i_particle_count_tests)

    add_executable(top_ui05sk217j_particle_emission_tests tests/SK217JParticleEmissionTests.cpp)
    target_include_directories(top_ui05sk217j_particle_emission_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217j_shared_snow_fire_emitter_timer COMMAND top_ui05sk217j_particle_emission_tests)

    add_executable(top_sk217ad_blast3_timeline_tests tests/SK217ADBlast3TimelineTests.cpp)
    target_include_directories(top_sk217ad_blast3_timeline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217ad_blast3_update_timeline COMMAND top_sk217ad_blast3_timeline_tests)

    add_executable(top_sk217ak_blast_group_tests tests/SK217AKBlastGroupTests.cpp)
    target_include_directories(top_sk217ak_blast_group_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME ui05sk217ak_blast_group_cycle COMMAND top_sk217ak_blast_group_tests)

    add_executable(top_sk217ak_blast_group_integration_tests tests/SK217AKBlastGroupIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217ak_blast_group_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217ak_blast_group_integration_tests PRIVATE z)
    add_test(NAME ui05sk217ak_blast_group_renderer COMMAND top_sk217ak_blast_group_integration_tests)

    add_executable(top_sk217ba_map_scene_effect_integration_tests tests/SK217BAMapSceneEffectIntegrationTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/ModelFile.cpp
        Engine/src/top/TexturePaths.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217ba_map_scene_effect_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217ba_map_scene_effect_integration_tests PRIVATE z)
    add_test(NAME sk217ba_map_scene_effect_controller COMMAND top_sk217ba_map_scene_effect_integration_tests)

endif()

# SK2.17-AO: item-dummy driven controller for historical DUMMY particles.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk217ao_dummy_controller_tests tests/SK217AODummyControllerTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk217ao_dummy_controller_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk217ao_dummy_controller_tests PRIVATE z)
    add_test(NAME sk217ao_dummy_controller COMMAND top_sk217ao_dummy_controller_tests)
endif()

# SK2.18-D: opt-in DX9 Part_trace target tracking and map collision.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk218d_part_trace_tests tests/SK218DPartTraceTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk218d_part_trace_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk218d_part_trace_tests PRIVATE z)
    add_test(NAME sk218d_part_trace_target_and_terrain COMMAND top_sk218d_part_trace_tests)
endif()

if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk218g_bound_billboard_tests tests/SK218GBoundBillboardTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk218g_bound_billboard_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk218g_bound_billboard_tests PRIVATE z)
    add_test(NAME sk218g_bound_billboard_center COMMAND top_sk218g_bound_billboard_tests)
endif()

add_executable(top_effect_binding_audit Tools/effect_binding_audit.cpp)
target_include_directories(top_effect_binding_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
add_executable(top_effect_unit_audit Tools/effect_unit_audit.cpp Engine/src/top/ModelFile.cpp)
target_include_directories(top_effect_unit_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
target_link_libraries(top_effect_unit_audit PRIVATE z)

# SK2.18-H: read-only authenticated scene-table audit; synthetic format fixtures.
if(TOP_BUILD_UI_TESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        execute_process(COMMAND ${Python3_EXECUTABLE} -c "import cryptography"
            RESULT_VARIABLE TOP_SCENE_AUDIT_CRYPTOGRAPHY OUTPUT_QUIET ERROR_QUIET)
    endif()
    if(Python3_Interpreter_FOUND AND TOP_SCENE_AUDIT_CRYPTOGRAPHY EQUAL 0)
        add_test(NAME sk218h_scene_table_audit
            COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scene_table_audit.py)
    else()
        message(STATUS "SK2.18-H audit test unavailable: requires Python3 and python3-cryptography")
    endif()
endif()

if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk218i_bound_model_matrix_tests tests/SK218IBoundModelMatrixTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk218i_bound_model_matrix_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk218i_bound_model_matrix_tests PRIVATE z)
    add_test(NAME sk218i_bound_model_matrix COMMAND top_sk218i_bound_model_matrix_tests)
endif()

if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk218j_binding_trace_tests tests/SK218JBindingTraceTests.cpp)
    target_include_directories(top_sk218j_binding_trace_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME sk218j_preview_binding_identity COMMAND top_sk218j_binding_trace_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME sk218j_binding_trace_audit COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_binding_trace_audit.py
            $<TARGET_FILE:top_sk218i_bound_model_matrix_tests> $<TARGET_FILE:top_skill_fx_probe>
            $<TARGET_FILE:top_sk218j_binding_trace_tests>)
    else()
        message(STATUS "SK2.18-J trace audit test unavailable: requires Python3")
    endif()
endif()

if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk218l_historical_defaults_tests tests/SK218LHistoricalDefaultsTests.cpp)
    target_include_directories(top_sk218l_historical_defaults_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME sk218l_normal_engine_policy COMMAND top_sk218l_historical_defaults_tests)

    add_executable(top_sk218k_eff_part_count_tests tests/SK218KEffPartCountTests.cpp
        Engine/src/top/EffectSystem.cpp Engine/src/top/TexturePaths.cpp
        Engine/src/top/ModelFile.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_sk218k_eff_part_count_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk218k_eff_part_count_tests PRIVATE z)
    add_test(NAME sk218k_authored_eff_part_count COMMAND top_sk218k_eff_part_count_tests)
    if(Python3_Interpreter_FOUND)
        add_test(NAME sk218k_eff_part_count_audit COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_eff_part_count_audit.py
            $<TARGET_FILE:top_fx_coverage_audit>)
    else()
        message(STATUS "SK2.18-K part-count audit test unavailable: requires Python3")
    endif()
endif()

# DEV-SEA-01: deterministic offline location persistence test (no GL/Client).
if(TOP_BUILD_UI_TESTS)
    add_executable(top_dev_world_position_tests tests/DevWorldPositionTests.cpp)
    target_include_directories(top_dev_world_position_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME dev_world_location_persistence COMMAND top_dev_world_position_tests)
endif()

# SEA-02: data-driven sea character Z, matching CCharacter::_UpdateHeight.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_historical_sea_height_tests tests/HistoricalCharacterHeightTests.cpp)
    target_include_directories(top_historical_sea_height_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME historical_sea_character_height COMMAND top_historical_sea_height_tests)
endif()

# SEA-03: operacoes de laboratorio sem renderer GL nem dependencias de efeitos.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_dev_sea_travel_tests tests/DevSeaTravelTests.cpp Engine/src/top/MapFile.cpp)
    target_include_directories(top_dev_sea_travel_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME sea03_dev_teleport_parser_and_terrain COMMAND top_dev_sea_travel_tests)
endif()

# SEA-03.1: regressao headless do casco restaurado e yaw exclusivo do F9.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_sea031_restoration_tests tests/Sea031RestorationTests.cpp)
    target_include_directories(top_sea031_restoration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME sea031_hull_restore_and_lab_heading COMMAND top_sea031_restoration_tests)
endif()

# PERF-01: validates stable monster identity independently of OpenGL/Client.
if(TOP_BUILD_UI_TESTS)
    add_executable(top_perf01_population_identity_tests tests/Perf01PopulationIdentityTests.cpp)
    target_include_directories(top_perf01_population_identity_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    add_test(NAME perf01_stable_spawn_identity COMMAND top_perf01_population_identity_tests)
endif()

if(TOP_BUILD_UI_TESTS)
    add_executable(top_sk220_embedded_effect_skin_tests tests/SK220EmbeddedEffectSkinTests.cpp Engine/src/top/ModelFile.cpp)
    target_include_directories(top_sk220_embedded_effect_skin_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
    target_link_libraries(top_sk220_embedded_effect_skin_tests PRIVATE z)
    add_test(NAME sk220_embedded_effect_bones COMMAND top_sk220_embedded_effect_skin_tests)
    set_tests_properties(sk220_embedded_effect_bones PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()

add_executable(top_pet_model_audit Tools/pet_model_audit.cpp Engine/src/top/ModelFile.cpp)
target_include_directories(top_pet_model_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
target_link_libraries(top_pet_model_audit PRIVATE z)
