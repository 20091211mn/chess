package com.ps4chess.app;

import android.content.Context;
import android.graphics.Canvas;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import com.ps4chess.app.ui.ChessApp;

/** The one and only view: it draws and feeds touches to ChessApp. */
final class ChessView extends View {
    private final ChessApp app;
    private final AndroidGfx gfx;

    ChessView(Context ctx, ChessApp app, AndroidGfx gfx) {
        super(ctx);
        this.app = app;
        this.gfx = gfx;
        setBackgroundColor(0xFF262522);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        app.resize(w, h);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        gfx.begin(canvas);
        app.draw(gfx, SystemClock.uptimeMillis());
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: app.touch(0, e.getX(), e.getY()); break;
            case MotionEvent.ACTION_MOVE: app.touch(1, e.getX(), e.getY()); break;
            case MotionEvent.ACTION_UP: app.touch(2, e.getX(), e.getY()); break;
            case MotionEvent.ACTION_CANCEL: app.touch(2, -1000, -1000); break;
            default: break;
        }
        invalidate();
        return true;
    }
}
