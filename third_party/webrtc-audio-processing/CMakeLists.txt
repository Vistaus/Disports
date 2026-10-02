# WebRTC's audio processing (BSD), the PulseAudio project's packaging of
# it: echo cancellation (AEC3), noise suppression and gain control for the
# microphone in calls. Ubuntu 24.04 only packages the 2016 version (0.3.1),
# whose echo canceller does poorly on phone loudspeakers. Built here from its
# meson files' source lists (for aarch64 and x86_64), against abseil.

set(WEBRTC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/upstream)

set(WEBRTC_SOURCES
    ${WEBRTC_DIR}/webrtc/api/audio/audio_frame.cc
    ${WEBRTC_DIR}/webrtc/api/audio/channel_layout.cc
    ${WEBRTC_DIR}/webrtc/api/audio/echo_canceller3_config.cc
    ${WEBRTC_DIR}/webrtc/api/audio_codecs/audio_decoder.cc
    ${WEBRTC_DIR}/webrtc/api/audio_codecs/audio_encoder.cc
    ${WEBRTC_DIR}/webrtc/api/rtp_headers.cc
    ${WEBRTC_DIR}/webrtc/api/rtp_packet_info.cc
    ${WEBRTC_DIR}/webrtc/api/task_queue/task_queue_base.cc
    ${WEBRTC_DIR}/webrtc/api/units/data_rate.cc
    ${WEBRTC_DIR}/webrtc/api/units/data_size.cc
    ${WEBRTC_DIR}/webrtc/api/units/frequency.cc
    ${WEBRTC_DIR}/webrtc/api/units/time_delta.cc
    ${WEBRTC_DIR}/webrtc/api/units/timestamp.cc
    ${WEBRTC_DIR}/webrtc/api/video/color_space.cc
    ${WEBRTC_DIR}/webrtc/api/video/hdr_metadata.cc
    ${WEBRTC_DIR}/webrtc/api/video/video_content_type.cc
    ${WEBRTC_DIR}/webrtc/api/video/video_timing.cc
    ${WEBRTC_DIR}/webrtc/common_audio/audio_converter.cc
    ${WEBRTC_DIR}/webrtc/common_audio/audio_util.cc
    ${WEBRTC_DIR}/webrtc/common_audio/channel_buffer.cc
    ${WEBRTC_DIR}/webrtc/common_audio/fir_filter_c.cc
    ${WEBRTC_DIR}/webrtc/common_audio/fir_filter_factory.cc
    ${WEBRTC_DIR}/webrtc/common_audio/real_fourier.cc
    ${WEBRTC_DIR}/webrtc/common_audio/real_fourier_ooura.cc
    ${WEBRTC_DIR}/webrtc/common_audio/resampler/push_resampler.cc
    ${WEBRTC_DIR}/webrtc/common_audio/resampler/push_sinc_resampler.cc
    ${WEBRTC_DIR}/webrtc/common_audio/resampler/resampler.cc
    ${WEBRTC_DIR}/webrtc/common_audio/resampler/sinc_resampler.cc
    ${WEBRTC_DIR}/webrtc/common_audio/resampler/sinusoidal_linear_chirp_source.cc
    ${WEBRTC_DIR}/webrtc/common_audio/ring_buffer.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/auto_corr_to_refl_coef.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/auto_correlation.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/complex_bit_reverse.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/complex_fft.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/copy_set_operations.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/cross_correlation.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/division_operations.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/dot_product_with_scale.cc
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/downsample_fast.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/energy.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/filter_ar.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/filter_ar_fast_q12.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/filter_ma_fast_q12.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/get_hanning_window.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/get_scaling_square.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/ilbc_specific_functions.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/levinson_durbin.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/lpc_to_refl_coef.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/min_max_operations.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/randomization_functions.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/real_fft.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/refl_coef_to_lpc.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/resample.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/resample_48khz.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/resample_by_2.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/resample_by_2_internal.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/resample_fractional.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/spl_init.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/spl_inl.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/spl_sqrt.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/splitting_filter.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/vector_scaling_operations.c
    ${WEBRTC_DIR}/webrtc/common_audio/smoothing_filter.cc
    ${WEBRTC_DIR}/webrtc/common_audio/third_party/ooura/fft_size_128/ooura_fft.cc
    ${WEBRTC_DIR}/webrtc/common_audio/third_party/ooura/fft_size_256/fft4g.cc
    ${WEBRTC_DIR}/webrtc/common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.c
    ${WEBRTC_DIR}/webrtc/common_audio/vad/vad.cc
    ${WEBRTC_DIR}/webrtc/common_audio/vad/vad_core.c
    ${WEBRTC_DIR}/webrtc/common_audio/vad/vad_filterbank.c
    ${WEBRTC_DIR}/webrtc/common_audio/vad/vad_gmm.c
    ${WEBRTC_DIR}/webrtc/common_audio/vad/vad_sp.c
    ${WEBRTC_DIR}/webrtc/common_audio/vad/webrtc_vad.c
    ${WEBRTC_DIR}/webrtc/common_audio/wav_file.cc
    ${WEBRTC_DIR}/webrtc/common_audio/wav_header.cc
    ${WEBRTC_DIR}/webrtc/common_audio/window_generator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines_hist.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/arith_routines_logist.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/audio_decoder_isac.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/audio_encoder_isac.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/bandwidth_estimator.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/crc.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/decode.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/decode_bwe.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/encode.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/encode_lpc_swb.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/entropy_coding.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/filter_functions.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/filterbanks.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/intialize.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/isac.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/isac_vad.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lattice.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_analysis.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_gain_swb_tables.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_shape_swb12_tables.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_shape_swb16_tables.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/lpc_tables.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_estimator.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_filter.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_gain_tables.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/pitch_lag_tables.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/spectrum_ar_model_tables.c
    ${WEBRTC_DIR}/webrtc/modules/audio_coding/codecs/isac/main/source/transform.c
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/adaptive_fir_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/adaptive_fir_filter_erl.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/aec3_common.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/aec3_fft.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/aec_state.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/alignment_mixer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/api_call_jitter_metrics.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/block_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/block_delay_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/block_framer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/block_processor.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/block_processor_metrics.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/clockdrift_detector.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/coarse_filter_update_gain.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/comfort_noise_generator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/decimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/dominant_nearend_detector.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/downsampled_render_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/echo_audibility.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/echo_canceller3.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/echo_path_delay_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/echo_path_variability.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/echo_remover.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/echo_remover_metrics.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/erl_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/erle_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/fft_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/filter_analyzer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/frame_blocker.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/fullband_erle_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/matched_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/matched_filter_lag_aggregator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/moving_average.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/refined_filter_update_gain.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/render_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/render_delay_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/render_delay_controller.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/render_delay_controller_metrics.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/render_signal_analyzer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/residual_echo_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/reverb_decay_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/reverb_frequency_response.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/reverb_model.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/reverb_model_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/signal_dependent_erle_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/spectrum_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/stationarity_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/subband_erle_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/subband_nearend_detector.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/subtractor.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/subtractor_output.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/subtractor_output_analyzer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/suppression_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/suppression_gain.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/transparent_mode.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec_dump/null_aec_dump_factory.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aecm/aecm_core.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aecm/aecm_core_c.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aecm/echo_control_mobile.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc/agc.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc/agc_manager_direct.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc/legacy/analog_agc.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc/legacy/digital_agc.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc/loudness_histogram.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc/utility.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/adaptive_agc.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/adaptive_digital_gain_applier.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/adaptive_mode_level_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/adaptive_mode_level_estimator_agc.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/agc2_testing_common.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/biquad_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/compute_interpolated_gain_curve.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/down_sampler.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/fixed_digital_level_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/gain_applier.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/interpolated_gain_curve.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/limiter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/limiter_db_gain_curve.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/noise_level_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/noise_spectrum_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/auto_correlation.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/common.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/features_extraction.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/lp_residual.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/pitch_search.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/pitch_search_internal.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/rnn.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/spectral_features.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/rnn_vad/spectral_features_internal.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/saturation_protector.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/signal_classifier.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/vad_with_level.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/agc2/vector_float_frame.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/audio_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/audio_processing_builder_impl.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/audio_processing_impl.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/echo_control_mobile_impl.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/echo_detector/circular_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/echo_detector/mean_variance_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/echo_detector/moving_max.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/echo_detector/normalized_covariance_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/gain_control_impl.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/gain_controller2.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/high_pass_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/include/aec_dump.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/include/audio_frame_proxies.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/include/audio_processing.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/include/audio_processing_statistics.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/include/config.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/level_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/logging/apm_data_dumper.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/fast_math.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/histograms.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/noise_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/noise_suppressor.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/ns_fft.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/prior_signal_model.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/prior_signal_model_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/quantile_noise_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/signal_model.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/signal_model_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/speech_probability_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/suppression_params.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/ns/wiener_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/optionally_built_submodule_creators.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/residual_echo_detector.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/rms_level.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/splitting_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/three_band_filter_bank.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/transient/file_utils.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/transient/moving_moments.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/transient/transient_detector.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/transient/transient_suppressor_impl.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/transient/wpd_node.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/transient/wpd_tree.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/typing_detection.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/utility/cascaded_biquad_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/utility/delay_estimator.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/utility/pffft_wrapper.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/vad/gmm.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/vad/pitch_based_vad.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/vad/pitch_internal.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/vad/pole_zero_filter.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/vad/standalone_vad.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/vad/vad_audio_proc.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/vad/vad_circular_buffer.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/vad/voice_activity_detector.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/voice_detection.cc
    ${WEBRTC_DIR}/webrtc/modules/third_party/fft/fft.c
    ${WEBRTC_DIR}/webrtc/rtc_base/checks.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/event.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/event_tracer.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/experiments/field_trial_parser.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/logging.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/memory/aligned_malloc.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/platform_thread.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/platform_thread_types.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/race_checker.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/string_encode.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/string_to_number.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/string_utils.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/strings/string_builder.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/synchronization/mutex.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/synchronization/rw_lock_posix.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/synchronization/rw_lock_wrapper.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/synchronization/yield.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/synchronization/yield_policy.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/system/file_wrapper.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/time_utils.cc
    ${WEBRTC_DIR}/webrtc/rtc_base/zero_memory.cc
    ${WEBRTC_DIR}/webrtc/system_wrappers/source/cpu_features.cc
    ${WEBRTC_DIR}/webrtc/system_wrappers/source/field_trial.cc
    ${WEBRTC_DIR}/webrtc/system_wrappers/source/metrics.cc
    ${WEBRTC_DIR}/webrtc/system_wrappers/source/sleep.cc
    ${WEBRTC_DIR}/webrtc/third_party/pffft/src/pffft.c
    ${WEBRTC_DIR}/webrtc/third_party/rnnoise/src/rnn_vad_weights.cc
)
# From the compiler (Clickable cross-compiles without setting
# CMAKE_SYSTEM_PROCESSOR).
set(WEBRTC_ARCH "${CMAKE_LIBRARY_ARCHITECTURE}")
if(NOT WEBRTC_ARCH)
    set(WEBRTC_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(WEBRTC_ARCH MATCHES "aarch64|arm64")
    list(APPEND WEBRTC_SOURCES
    ${WEBRTC_DIR}/webrtc/common_audio/fir_filter_neon.cc
    ${WEBRTC_DIR}/webrtc/common_audio/resampler/sinc_resampler_neon.cc
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/cross_correlation_neon.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/downsample_fast_neon.c
    ${WEBRTC_DIR}/webrtc/common_audio/signal_processing/min_max_operations_neon.c
    ${WEBRTC_DIR}/webrtc/common_audio/third_party/ooura/fft_size_128/ooura_fft_neon.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aecm/aecm_core_neon.cc
    )
    set(WEBRTC_ARCH_DEFINES WEBRTC_ARCH_ARM64 WEBRTC_HAS_NEON)
elseif(WEBRTC_ARCH MATCHES "x86_64|AMD64")
    set(WEBRTC_SSE2_SOURCES
    ${WEBRTC_DIR}/webrtc/common_audio/fir_filter_sse.cc
    ${WEBRTC_DIR}/webrtc/common_audio/resampler/sinc_resampler_sse.cc
    ${WEBRTC_DIR}/webrtc/common_audio/third_party/ooura/fft_size_128/ooura_fft_sse2.cc
    )
    set(WEBRTC_AVX2_SOURCES
    ${WEBRTC_DIR}/webrtc/common_audio/fir_filter_avx2.cc
    ${WEBRTC_DIR}/webrtc/common_audio/resampler/sinc_resampler_avx2.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/adaptive_fir_filter_avx2.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/adaptive_fir_filter_erl_avx2.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/fft_data_avx2.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/matched_filter_avx2.cc
    ${WEBRTC_DIR}/webrtc/modules/audio_processing/aec3/vector_math_avx2.cc
    )
    list(APPEND WEBRTC_SOURCES

        ${WEBRTC_SSE2_SOURCES} ${WEBRTC_AVX2_SOURCES}
    )
    set_source_files_properties(${WEBRTC_SSE2_SOURCES} PROPERTIES COMPILE_OPTIONS "-msse2")
    set_source_files_properties(${WEBRTC_AVX2_SOURCES} PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
    set(WEBRTC_ARCH_DEFINES WEBRTC_ENABLE_AVX2)
else()
    message(FATAL_ERROR "webrtc-audio-processing: no source list for ${WEBRTC_ARCH}")
endif()

add_library(webrtc_audio_processing STATIC ${WEBRTC_SOURCES})
target_include_directories(webrtc_audio_processing PUBLIC ${WEBRTC_DIR}/webrtc)
target_compile_definitions(webrtc_audio_processing
    PUBLIC WEBRTC_POSIX WEBRTC_LINUX WEBRTC_THREAD_RR ${WEBRTC_ARCH_DEFINES}
    PRIVATE WEBRTC_LIBRARY_IMPL WEBRTC_APM_DEBUG_DUMP=0 NDEBUG _GNU_SOURCE
)
target_compile_features(webrtc_audio_processing PUBLIC cxx_std_17)
# Newer GCC (Ubuntu 26.04) no longer pulls <cstdint> in through other
# headers, and this WebRTC release relies on it.
target_compile_options(webrtc_audio_processing PRIVATE -w "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdint>")
target_link_libraries(webrtc_audio_processing PUBLIC absl::base absl::strings absl::optional Threads::Threads)
set_target_properties(webrtc_audio_processing PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC OFF)
