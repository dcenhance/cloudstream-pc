#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_ROOT="$ROOT/build-tests"
run_test() {
  name=$1
  project=$2
  binary=$3
  dir="$BUILD_ROOT/$name"
  mkdir -p "$dir"
  cd "$dir"
  qmake6 "$project" CONFIG+=release
  make -j"${JOBS:-2}"
  "./$binary" -txt
}
run_test repository-url "$ROOT/tests/repository-url.pro" test_repository_url_resolver
run_test repository-manifest "$ROOT/tests/repository-manifest.pro" test_repository_manifest_parser
run_test player-command "$ROOT/tests/player-command.pro" test_player_command
run_test watch-history "$ROOT/tests/watch-history.pro" test_watch_history_store
run_test mpv-ipc-protocol "$ROOT/tests/mpv-ipc-protocol.pro" test_mpv_ipc_protocol
run_test process-completion "$ROOT/tests/process-completion.pro" test_process_completion
run_test extension-registry "$ROOT/tests/extension-registry.pro" test_extension_registry
run_test artwork-sizing "$ROOT/tests/artwork-sizing.pro" test_artwork_sizing
run_test artwork-loader "$ROOT/tests/artwork-loader.pro" test_artwork_loader
run_test provider-discovery-generation "$ROOT/tests/provider-discovery-generation.pro" test_provider_discovery_generation
run_test provider-selection-model "$ROOT/tests/provider-selection-model.pro" test_provider_selection_model
run_test provider-picker-dialog "$ROOT/tests/provider-picker-dialog.pro" test_provider_picker_dialog
run_test provider-validation "$ROOT/tests/provider-validation.pro" test_provider_validation
run_test home-process-result "$ROOT/tests/home-process-result.pro" test_home_process_result
run_test home-content-limiter "$ROOT/tests/home-content-limiter.pro" test_home_content_limiter
run_test home-hero-selection "$ROOT/tests/home-hero-selection.pro" test_home_hero_selection
run_test provider-configuration "$ROOT/tests/provider-configuration.pro" test_provider_configuration
run_test provider-preference-filter "$ROOT/tests/provider-preference-filter.pro" test_provider_preference_filter
run_test search-history-model "$ROOT/tests/search-history-model.pro" test_search_history_model
run_test settings-pane "$ROOT/tests/settings-pane.pro" test_settings_pane
run_test network-request-policy "$ROOT/tests/network-request-policy.pro" test_network_request_policy
run_test smooth-scroll-controller "$ROOT/tests/smooth-scroll-controller.pro" test_smooth_scroll_controller
run_test gamepad-navigation "$ROOT/tests/gamepad-navigation.pro" test_gamepad_navigation
run_test extension-list-filter "$ROOT/tests/extension-list-filter.pro" test_extension_list_filter
run_test extension-install-batch "$ROOT/tests/extension-install-batch.pro" test_extension_install_batch
run_test details-presentation "$ROOT/tests/details-presentation.pro" test_details_presentation
run_test episode-catalog "$ROOT/tests/episode-catalog.pro" test_episode_catalog
run_test source-catalog "$ROOT/tests/source-catalog.pro" test_source_catalog
run_test download-manager "$ROOT/tests/download-manager.pro" test_download_manager
run_test mpv-player-widget "$ROOT/tests/mpv-player-widget.pro" test_mpv_player_widget
