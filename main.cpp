#include <bits/stdc++.h>
#include <SFML/Graphics.hpp>
using namespace std;
using namespace sf;

// y^2 = x^3 + a*x + b
// halb 3, tangent case ist noch n bisschen eklig

double a = -1, b = 1;
double px, py, qx, qy;
double ox, oy, sc;          // screen origin + px per unit
bool playing = 0, cons = 0;
float anim0 = 0;
int drag = 0;               // 0 nix, 1 P, 2 Q, 3 pan
int lx, ly;
bool fit = 0;

double f(double x) { return x*x*x + a*x + b; }

double clampd(double t){ return t<0?0:(t>1?1:t); }
double ease(double t){
    t = clampd(t);
    if(t<0.5) return 2*t*t;
    double u = -2*t+2;
    return 1 - u*u/2;
}

struct Pnt { double x,y; bool inf; };

Pnt addp(Pnt P, Pnt Q){
    Pnt r{0,0,0};
    if(fabs(P.x-Q.x)<1e-9 && fabs(P.y+Q.y)<1e-8){ r.inf=1; return r; }
    double lam;
    if(fabs(P.x-Q.x)<1e-12 && fabs(P.y-Q.y)<1e-12){
        if(fabs(P.y)<1e-10){ r.inf=1; return r; }
        lam = (3*P.x*P.x + a) / (2*P.y);
    }else lam = (Q.y-P.y)/(Q.x-P.x);
    double x3 = lam*lam - P.x - Q.x;
    double yl = P.y + lam*(x3-P.x);
    r.x=x3; r.y=-yl; r.inf=0;
    return r;
}

// third hit of the line, i.e. -(P+Q)
Pnt negR(Pnt P, Pnt Q, bool &vert, bool &dbl){
    vert=0; dbl=0;
    Pnt z{0,0,0};
    double dx=Q.x-P.x, dy=Q.y-P.y;
    bool same = dx*dx+dy*dy < 1e-16;
    dbl = same;
    if(fabs(dx)<1e-9 && fabs(P.y+Q.y)<1e-8){ vert=1; z.inf=1; return z; }
    double lam = same ? (3*P.x*P.x+a)/(2*P.y) : dy/dx;
    if(!isfinite(lam)){ vert=1; z.inf=1; return z; }
    z.x = lam*lam - P.x - Q.x;
    z.y = P.y + lam*(z.x - P.x);
    z.inf=0;
    return z;
}

void lift(double &x, double &y){
    int s = y<0 ? -1 : 1;
    auto ok=[&](double xx){ double r=f(xx); return r>=0 && isfinite(r); };
    if(ok(x)){ y = s*sqrt(f(x)); return; }
    for(int i=1;i<800;i++){
        double d=i*0.015;
        if(ok(x+d)){ x+=d; y=s*sqrt(f(x)); return; }
        if(ok(x-d)){ x-=d; y=s*sqrt(f(x)); return; }
    }
}

Vector2f S(double x, double y){
    return Vector2f((float)(ox + x*sc), (float)(oy - y*sc));
}
Vector2f Wld(int sx, int sy){
    return Vector2f((float)((sx-ox)/sc), (float)((oy-sy)/sc));
}

void nearest(int mx, int my, double &x, double &y){
    double xL = (0-ox)/sc, xR = (1100-ox)/sc; // eh, wird unten mit echter w/h ueberschrieben
    (void)xL; (void)xR;
}

// echte version, fensterbreite kennt der caller
void snapPt(int mx, int my, int W, int H, double &x, double &y){
    double xL = (0 - ox)/sc;
    double xR = (W - ox)/sc;
    int n = 500;
    double bx=x, by=y, bd=1e18;
    for(int i=0;i<=n;i++){
        double xx = xL + (xR-xL)*i/n;
        double r = f(xx);
        if(r<0 || !isfinite(r)) continue;
        double yy = sqrt(r);
        for(int k=0;k<2;k++){
            double yk = k? -yy : yy;
            auto s = S(xx, yk);
            double pix = (s.x-mx)*(s.x-mx) + (s.y-my)*(s.y-my);
            double cont = fabs(xx-x)*sc*0.25;
            double d = pix + cont*cont;
            if(d<bd){ bd=d; bx=xx; by=yk; }
        }
    }
    x=bx; y=by;
}

void line(RenderWindow &w, Vector2f A, Vector2f B, Color c, float th=1.2f){
    Vertex v[2] = { Vertex(A,c), Vertex(B,c) };
    // sfml 2: thickness geht so nicht, also duenne linie. reicht.
    w.draw(v, 2, Lines);
    (void)th;
}

void disc(RenderWindow &w, Vector2f p, float rad, Color fill, Color out){
    CircleShape c(rad);
    c.setOrigin(rad, rad);
    c.setPosition(p);
    c.setFillColor(fill);
    c.setOutlineColor(out);
    c.setOutlineThickness(1.2f);
    w.draw(c);
}

double gridStep(){
    double raw = 84.0 / max(sc, 1e-12);
    double pw = pow(10, floor(log10(raw)));
    double n = raw / pw;
    double m = 1;
    if(n>5) m=10;
    else if(n>2.2) m=5;
    else if(n>1.15) m=2;
    return m*pw;
}

int main(){
    ios::sync_with_stdio(false);
    px = -1.2; py = sqrt(max(0.0, f(px)));
    qx =  1.5; qy = sqrt(max(0.0, f(qx)));

    RenderWindow win(VideoMode(1100, 720), "ec");
    win.setFramerateLimit(60);

    Font font;
    // dejavu liegt auf den meisten linuxen, sonst bleibt die schrift halt default
    font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf");

    Clock clk;
    string as = "-1", bs = "1";
    int editing = 0; // 1 = a, 2 = b

    while(win.isOpen()){
        int W = win.getSize().x, H = win.getSize().y;
        if(!fit){
            ox = W*0.5 - 0.2*90;
            oy = H*0.5 + 0.05*90;
            sc = min(W,H) / 6.2;
            // origin so world (0.2, 0.05) sits near center
            ox = W*0.5 - 0.2*sc;
            oy = H*0.5 + 0.05*sc;
            fit = 1;
        }

        Event e;
        while(win.pollEvent(e)){
            if(e.type==Event::Closed) win.close();
            if(e.type==Event::Resized){
                win.setView(View(FloatRect(0,0,e.size.width, e.size.height)));
            }
            if(e.type==Event::MouseWheelScrolled){
                double mx=e.mouseWheelScroll.x, my=e.mouseWheelScroll.y;
                double wx=(mx-ox)/sc, wy=(oy-my)/sc;
                double fct = exp(e.mouseWheelScroll.delta * 0.12);
                sc = min(1e6, max(1e-16, sc*fct));
                ox = mx - wx*sc;
                oy = my + wy*sc;
            }
            if(e.type==Event::MouseButtonPressed && e.mouseButton.button==Mouse::Left){
                int mx=e.mouseButton.x, my=e.mouseButton.y;
                // play button, oben links
                if(mx<58 && my<58){
                    playing=1; cons=0; anim0=clk.getElapsedTime().asSeconds();
                }else{
                    auto sp=S(px,py), sq=S(qx,qy);
                    double dp=hypot(sp.x-mx, sp.y-my);
                    double dq=hypot(sq.x-mx, sq.y-my);
                    if(dp<16 && dp<=dq){ drag=1; playing=0; cons=0; }
                    else if(dq<16){ drag=2; playing=0; cons=0; }
                    else { drag=3; lx=mx; ly=my; }
                }
            }
            if(e.type==Event::MouseButtonPressed && e.mouseButton.button==Mouse::Right){
                int mx=e.mouseButton.x, my=e.mouseButton.y;
                auto sp=S(px,py), sq=S(qx,qy);
                if(hypot(sp.x-mx,sp.y-my)<16) { py=-py; playing=0; cons=0; }
                else if(hypot(sq.x-mx,sq.y-my)<16){ qy=-qy; playing=0; cons=0; }
            }
            if(e.type==Event::MouseButtonReleased){
                if(drag==1 || drag==2){
                    auto sp=S(px,py), sq=S(qx,qy);
                    if(hypot(sp.x-sq.x, sp.y-sq.y)<14){
                        if(drag==1){ px=qx; py=qy; }
                        else { qx=px; qy=py; }
                    }
                }
                drag=0;
            }
            if(e.type==Event::MouseMoved && drag){
                int mx=e.mouseMove.x, my=e.mouseMove.y;
                if(drag==3){
                    ox += mx-lx; oy += my-ly; lx=mx; ly=my;
                }else if(drag==1) snapPt(mx,my,W,H,px,py);
                else if(drag==2) snapPt(mx,my,W,H,qx,qy);
            }
            if(e.type==Event::KeyPressed){
                if(e.key.code==Keyboard::Equal){
                    double mx=W/2.0, my=H/2.0, wx=(mx-ox)/sc, wy=(oy-my)/sc;
                    sc=min(1e6, sc*1.18);
                    ox=mx-wx*sc; oy=my+wy*sc;
                }
                if(e.key.code==Keyboard::Hyphen){
                    double mx=W/2.0, my=H/2.0, wx=(mx-ox)/sc, wy=(oy-my)/sc;
                    sc=max(1e-16, sc/1.18);
                    ox=mx-wx*sc; oy=my+wy*sc;
                }
                // doppelklick-ersatz
                if(e.key.code==Keyboard::R){
                    sc = min(W,H)/6.2;
                    ox = W*0.5 - 0.2*sc;
                    oy = H*0.5 + 0.05*sc;
                }
            }
            if(e.type==Event::TextEntered && editing){
                char c = (char)e.text.unicode;
                string &s = editing==1 ? as : bs;
                if(c==8 || c==127){ if(s.size()) s.pop_back(); }
                else if((c>='0'&&c<='9')||c=='-'||c=='.'||c==',') s.push_back(c==','?'.':c);
                char *end=0;
                double v = strtod(s.c_str(), &end);
                if(end && *end==0 && s.size() && s!="-" && s!="." && s!="-."){
                    if(editing==1) a=v; else b=v;
                    lift(px,py); lift(qx,qy);
                    playing=0; cons=0;
                }
            }
            // a / b umschalten: taste A oder B, bisschen billig aber ohne slider
            if(e.type==Event::KeyPressed && e.key.code==Keyboard::A) editing = editing==1?0:1;
            if(e.type==Event::KeyPressed && e.key.code==Keyboard::B) editing = editing==2?0:2;
        }

        // bg
        win.clear(Color(7, 12, 22));

        // grid
        double step = gridStep();
        double xL=(0-ox)/sc, xR=(W-ox)/sc;
        double yB=(oy-H)/sc, yT=(oy-0)/sc;
        for(double x=ceil(xL/step)*step; x<xR; x+=step){
            auto p=S(x,0);
            Color gc = fabs(x)<step*1e-4 ? Color(176,198,220,110) : Color(148,176,206,28);
            line(win, Vector2f(p.x,0), Vector2f(p.x,(float)H), gc);
        }
        for(double y=ceil(yB/step)*step; y<yT; y+=step){
            auto p=S(0,y);
            Color gc = fabs(y)<step*1e-4 ? Color(176,198,220,110) : Color(148,176,206,28);
            line(win, Vector2f(0,p.y), Vector2f((float)W,p.y), gc);
        }
        // achsen nochmal drueber, falls 0 nicht auf nem step lag
        {
            auto o=S(0,0);
            line(win, Vector2f(0,o.y), Vector2f((float)W,o.y), Color(176,198,220,120));
            line(win, Vector2f(o.x,0), Vector2f(o.x,(float)H), Color(176,198,220,120));
        }

        // kurve, pro spalte
        VertexArray up(LineStrip), dn(LineStrip);
        for(int sx=0; sx<=W; sx++){
            double x = (sx - ox)/sc;
            double r = f(x);
            if(r<0 || !isfinite(r)){
                if(up.getVertexCount()){ win.draw(up); up.clear(); }
                if(dn.getVertexCount()){ win.draw(dn); dn.clear(); }
                continue;
            }
            double yy=sqrt(r);
            up.append(Vertex(S(x, yy), Color(197,213,230)));
            dn.append(Vertex(S(x,-yy), Color(197,213,230)));
        }
        if(up.getVertexCount()) win.draw(up);
        if(dn.getVertexCount()) win.draw(dn);

        Pnt P{px,py,0}, Q{qx,qy,0};
        auto sp=S(px,py), sq=S(qx,qy);
        bool sameScr = hypot(sp.x-sq.x, sp.y-sq.y) < 12;
        if(sameScr) Q=P;

        bool vert=0, dbl=0;
        Pnt nr = negR(P,Q,vert,dbl);
        Pnt R = addp(P,Q);

        float now = clk.getElapsedTime().asSeconds();
        float u = 0;
        if(playing){
            u = clampd( (now-anim0)/3.4 );
            if(u>=1){ playing=0; cons=1; u=1; }
        }else if(cons) u=1;
        bool show = playing || cons;

        auto seg=[&](Pnt A, Pnt B, Color c){
            line(win, S(A.x,A.y), S(B.x,B.y), c);
        };

        if(show && u>0){
            if(vert){
                double gow = ease(u/0.62);
                double yTop=yT+1, yBot=yB-1;
                Pnt A{P.x, P.y+(yTop-P.y)*gow, 0};
                Pnt B{P.x, P.y+(yBot-P.y)*gow, 0};
                seg(A,B, Color(190,208,224,190));
            }else if(!nr.inf){
                Pnt neg = nr;
                if(dbl){
                    double t=ease(clampd(u/0.48));
                    Pnt back{P.x-(neg.x-P.x)*0.22, P.y-(neg.y-P.y)*0.22, 0};
                    Pnt tip{P.x+(neg.x-P.x)*t, P.y+(neg.y-P.y)*t, 0};
                    Pnt bk{P.x+(back.x-P.x)*t, P.y+(back.y-P.y)*t, 0};
                    seg(bk, tip, Color(190,208,224,190));
                }else{
                    double t1=ease(clampd(u/0.38));
                    Pnt tip{P.x+(Q.x-P.x)*t1, P.y+(Q.y-P.y)*t1, 0};
                    // Q kann gesnappt sein, sichtbare punkte sind px/qx
                    Pnt Pvis{px,py,0}, Qvis{qx,qy,0};
                    tip = Pnt{Pvis.x+(Qvis.x-Pvis.x)*t1, Pvis.y+(Qvis.y-Pvis.y)*t1, 0};
                    seg(Pvis, tip, Color(190,208,224,200));
                    double dx=Qvis.x-Pvis.x, dy=Qvis.y-Pvis.y;
                    double l2=dx*dx+dy*dy;
                    double tn = l2<1e-16? 0 : ((neg.x-Pvis.x)*dx+(neg.y-Pvis.y)*dy)/l2;
                    bool mid = tn>0.04 && tn<0.96;
                    if(!mid && u>0.38){
                        double t2=ease(clampd((u-0.38)/0.22));
                        double dP=hypot(neg.x-Pvis.x, neg.y-Pvis.y);
                        double dQ=hypot(neg.x-Qvis.x, neg.y-Qvis.y);
                        Pnt from = dQ<=dP ? Qvis : Pvis;
                        Pnt to{from.x+(neg.x-from.x)*t2, from.y+(neg.y-from.y)*t2, 0};
                        seg(from, to, Color(190,208,224,180));
                    }
                }
                if(u>0.6 && !R.inf){
                    double t3=ease(clampd((u-0.6)/0.28));
                    Pnt to{neg.x+(R.x-neg.x)*t3, neg.y+(R.y-neg.y)*t3, 0};
                    seg(neg, to, Color(198,214,228,210));
                }
            }
        }

        // ergebnis
        if(R.inf || vert){
            float al = playing ? (float)clampd((u-0.5)/0.28) : 0.9f;
            if(al>0.05f){
                Text inf;
                inf.setFont(font);
                inf.setString("R = inf");
                inf.setCharacterSize(16);
                inf.setFillColor(Color(213,226,240, (Uint8)(al*255)));
                auto s=S(px,0);
                inf.setPosition(s.x-20, 64);
                win.draw(inf);
            }
        }else{
            float al = playing ? (float)clampd((u-0.78)/0.16) : 1.f;
            if(al>0.02f){
                auto s=S(R.x,R.y);
                CircleShape halo(10);
                halo.setOrigin(10,10);
                halo.setPosition(s);
                halo.setFillColor(Color::Transparent);
                halo.setOutlineColor(Color(198,214,228, (Uint8)(110*al)));
                halo.setOutlineThickness(1);
                win.draw(halo);
                disc(win, s, 5, Color(228,237,245,(Uint8)(255*al)), Color(142,168,192));
                Text lab; lab.setFont(font); lab.setString("R");
                lab.setCharacterSize(16); lab.setStyle(Text::Italic);
                lab.setFillColor(Color(215,226,238,(Uint8)(255*al)));
                lab.setPosition(s.x+10, s.y-22);
                win.draw(lab);
            }
        }

        disc(win, sq, 5.4f, Color(169,191,210), Color(109,138,166));
        disc(win, sp, 5.4f, Color(231,238,246), Color(144,168,192));

        auto tag=[&](const char* name, Vector2f p, float dx){
            Text t; t.setFont(font); t.setString(name);
            t.setCharacterSize(16); t.setStyle(Text::Italic);
            t.setFillColor(Color(215,226,238));
            t.setPosition(p.x+dx, p.y-22);
            win.draw(t);
        };
        tag("P", sp, 10);
        tag("Q", sq, sameScr ? -18.f : 10.f);

        // play, oben links. kein slider, nix drunter.
        RectangleShape btn(Vector2f(44,44));
        btn.setPosition(14,14);
        btn.setFillColor(Color(8,16,30,210));
        btn.setOutlineColor(Color(190,210,230,70));
        btn.setOutlineThickness(1);
        win.draw(btn);
        ConvexShape tri;
        tri.setPointCount(3);
        tri.setPoint(0, Vector2f(31, 28));
        tri.setPoint(1, Vector2f(31, 44));
        tri.setPoint(2, Vector2f(45, 36));
        tri.setFillColor(Color(215,227,241));
        win.draw(tri);
        if(playing){
            // bogen als viele kleine linien, zu faul fuer arc
            float ang = u * 6.28318f;
            Vector2f c0(36,36);
            VertexArray arc(LineStrip);
            for(int i=0;i<=32;i++){
                float a1 = -1.5708f + ang*i/32.f;
                arc.append(Vertex(Vector2f(c0.x+19*cos(a1), c0.y+19*sin(a1)), Color(214,226,238)));
            }
            win.draw(arc);
        }

        // gleichung oben, a/b tippen nach A bzw B
        string eq = "y^2 = x^3 + (" + as + ")x + (" + bs + ")";
        if(editing==1) eq += "   [a]";
        if(editing==2) eq += "   [b]";
        Text et; et.setFont(font); et.setString(eq);
        et.setCharacterSize(18); et.setFillColor(Color(197,211,228));
        et.setPosition(78, 24);
        win.draw(et);

        win.display();
    }
}
