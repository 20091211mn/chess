import com.ps4chess.app.ui.Gfx;
import java.awt.*;
import java.awt.font.FontRenderContext;
import java.awt.geom.*;
import java.awt.image.BufferedImage;
import java.io.File;
import java.util.ArrayDeque;

/** Gfx on Java2D so every screen of the Android app can be rendered to a PNG in tests. */
public final class Java2DGfx implements Gfx {
    final BufferedImage img; final Graphics2D g;
    private final Font reg, bold;
    private final ArrayDeque<AffineTransform> tx = new ArrayDeque<AffineTransform>();
    private final ArrayDeque<Shape> clips = new ArrayDeque<Shape>();

    public Java2DGfx(int w, int h, String fontDir) throws Exception {
        img = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB);
        g = img.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL, RenderingHints.VALUE_STROKE_PURE);
        reg = Font.createFont(Font.TRUETYPE_FONT, new File(fontDir, "Poppins-Medium.ttf"));
        bold = Font.createFont(Font.TRUETYPE_FONT, new File(fontDir, "Poppins-Bold.ttf"));
    }

    private static Color c(int argb) { return new Color(argb, true); }
    private static Path2D poly(float[] p) { Path2D.Float s = new Path2D.Float(); s.moveTo(p[0], p[1]); for (int i = 2; i + 1 < p.length; i += 2) s.lineTo(p[i], p[i + 1]); s.closePath(); return s; }

    public void fillRect(float x, float y, float w, float h, int a) { g.setPaint(c(a)); g.fill(new Rectangle2D.Float(x, y, w, h)); }
    public void fillRound(float x, float y, float w, float h, float r, int a) { g.setPaint(c(a)); g.fill(new RoundRectangle2D.Float(x, y, w, h, r * 2, r * 2)); }
    public void strokeRound(float x, float y, float w, float h, float r, float sw, int a) { g.setPaint(c(a)); g.setStroke(new BasicStroke(sw)); g.draw(new RoundRectangle2D.Float(x + sw / 2, y + sw / 2, w - sw, h - sw, r * 2, r * 2)); }
    public void fillCircle(float cx, float cy, float r, int a) { g.setPaint(c(a)); g.fill(new Ellipse2D.Float(cx - r, cy - r, 2 * r, 2 * r)); }
    public void strokeCircle(float cx, float cy, float r, float sw, int a) { g.setPaint(c(a)); g.setStroke(new BasicStroke(sw)); g.draw(new Ellipse2D.Float(cx - r, cy - r, 2 * r, 2 * r)); }
    public void fillOval(float cx, float cy, float rx, float ry, int a) { g.setPaint(c(a)); g.fill(new Ellipse2D.Float(cx - rx, cy - ry, 2 * rx, 2 * ry)); }
    public void fillPoly(float[] p, int a) { g.setPaint(c(a)); g.fill(poly(p)); }
    public void strokePoly(float[] p, float sw, int a) { g.setPaint(c(a)); g.setStroke(new BasicStroke(sw, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND)); g.draw(poly(p)); }
    public void fillPolyGrad(float[] p, float y0, float y1, int top, int bot) { g.setPaint(new GradientPaint(0, y0, c(top), 0, y1, c(bot))); g.fill(poly(p)); }

    public void text(String s, float x, float y, float size, int a, boolean b, int align) {
        Font f = (b ? bold : reg).deriveFont(size);
        g.setFont(f); g.setPaint(c(a));
        FontMetrics fm = g.getFontMetrics();
        float w = fm.stringWidth(s), bx = align == CENTER ? x - w / 2 : align == RIGHT ? x - w : x;
        float base = y + (fm.getAscent() - fm.getDescent()) / 2f;
        g.drawString(s, bx, base);
    }
    public float textWidth(String s, float size, boolean b) { g.setFont((b ? bold : reg).deriveFont(size)); return g.getFontMetrics().stringWidth(s); }
    public void save() { tx.push(g.getTransform()); clips.push(g.getClip() == null ? new Rectangle2D.Float(-1e5f, -1e5f, 2e5f, 2e5f) : g.getClip()); }
    public void translate(float x, float y) { g.translate(x, y); }
    public void scale(float s) { g.scale(s, s); }
    public void restore() { g.setTransform(tx.pop()); g.setClip(clips.pop()); }
    public void clip(float x, float y, float w, float h) { g.clip(new Rectangle2D.Float(x, y, w, h)); }
}
