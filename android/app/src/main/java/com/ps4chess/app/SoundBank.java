package com.ps4chess.app;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;

import com.ps4chess.app.ui.Platform;

/** Tiny synthesized move sounds (no audio files needed). */
final class SoundBank {
    private static final int RATE = 22050;
    private final AudioTrack[] tracks = new AudioTrack[7];

    SoundBank() {
        try {
            tracks[Platform.SND_MOVE] = make(new double[][] {{330, 0.07}}, 0.5);
            tracks[Platform.SND_CAPTURE] = make(new double[][] {{200, 0.06}, {130, 0.10}}, 0.6);
            tracks[Platform.SND_CHECK] = make(new double[][] {{660, 0.09}, {880, 0.15}}, 0.5);
            tracks[Platform.SND_CASTLE] = make(new double[][] {{330, 0.05}, {0, 0.03}, {330, 0.05}}, 0.5);
            tracks[Platform.SND_END] = make(new double[][] {{440, 0.2}, {554, 0.2}, {659, 0.35}}, 0.45);
            tracks[Platform.SND_ILLEGAL] = make(new double[][] {{110, 0.12}}, 0.35);
            tracks[Platform.SND_NOTIFY] = make(new double[][] {{740, 0.08}, {990, 0.12}}, 0.45);
        } catch (Throwable ignored) { }
    }

    private AudioTrack make(double[][] notes, double vol) {
        int total = 0;
        for (double[] n : notes) total += (int) (n[1] * RATE);
        short[] pcm = new short[total];
        int at = 0;
        for (double[] n : notes) {
            int len = (int) (n[1] * RATE);
            for (int i = 0; i < len; i++) {
                double env = Math.min(1.0, i / 120.0) * Math.pow(1.0 - (double) i / len, 1.6);
                pcm[at + i] = (short) (n[0] == 0 ? 0 : Math.sin(2 * Math.PI * n[0] * i / RATE) * env * vol * 24000);
            }
            at += len;
        }
        AudioTrack t = new AudioTrack(AudioManager.STREAM_MUSIC, RATE, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT, pcm.length * 2, AudioTrack.MODE_STATIC);
        t.write(pcm, 0, pcm.length);
        return t;
    }

    void play(int kind) {
        try {
            AudioTrack t = kind >= 0 && kind < tracks.length ? tracks[kind] : null;
            if (t == null) return;
            t.stop();
            t.reloadStaticData();
            t.play();
        } catch (Throwable ignored) { }
    }
}
