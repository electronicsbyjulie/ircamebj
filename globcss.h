
char* global_css = R"GLOBCSS(
* {
           background-color: black;
           color: #fffec0;
           font-size: 32px;
           font-weight: 800;
    }
    .redbk, .redbk *
    {
       background-color: #990000;
    }
    .pwr
    {
       background-image: url("/home/pi/pwr.png");
       background-color: transparent;
    }
    .pwrr
    {
       background-image: url("/home/pi/pwrr.png");
       background-color: transparent;
    }
    .fire
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#000000),
                           color-stop (0.25, #0000ff),
                           color-stop (0.33, #990099),
                           color-stop (0.5, #ff0000),
                           color-stop (0.75, #ffff00),
                           to (#ffffff)
                                      );
       color: #000;
    }
    
    
    .fever
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#000000),
                           color-stop (0.50, #0000ff),
                           color-stop (0.55, #00ff00),
                           color-stop (0.60, #ff0000),
                           to (#000000)
                                      );
       color: #fff;
    }
    
    .room
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#0000ff),
                           color-stop (0.50, #00ff00),
                           to (#ff0000)
                                      );
       color: #fff;
    }
    
    .amb
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#0000ff),
                           color-stop (0.15, #009999),
                           color-stop (0.50, #00ff00),
                           color-stop (0.85, #999900),
                           to (#ff0000)
                                      );
       color: #fff;
    }
    
    .rainb
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#660066),
                           color-stop (0.20, #0000ff),
                           color-stop (0.40, #00ff00),
                           color-stop (0.60, #ffcc00),
                           color-stop (0.80, #ff0066),
                           to (#ccccff)
                                      );
       color: #fff;
    }
    
    .lava
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#FF3300),
                           color-stop (0.33, #FF6600),
                           color-stop (0.67, #FF9900),
                           to (#FFCC00)
                                      );
       color: #000;
    }
    
    .bleu
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#000000),
                           color-stop (0.25, #003333),
                           color-stop (0.33, #336666),
                           color-stop (0.51, #99ffff),
                           color-stop (0.75, #ffeeee),
                           to (#ffffff)
                                      );
       color: #000;
    }
    .tiv
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#0099ff),
                           to (#ff9966)
                                      );
       color: #000;
    }
    .hues
    {
       background-image: -gtk-gradient(linear,
                                       left top,
                                       right top,
                           from (#000),
                           color-stop (0.16, #0000ff),
                           color-stop (0.24, #00cccc),
                           color-stop (0.33, #00ff00),
                           color-stop (0.50, #ff00ff),
                           color-stop (0.67, #ffff00),
                           color-stop (0.83, #ff0000),
                           to (#fff)
                                      );
       color: #000;
    }
    .rgi
    {
       background-image: url("/home/pi/rgi.png");
       background-color: transparent;
    }
    .cir
    {
       background-image: url("/home/pi/cir.png");
       background-color: transparent;
    }
    .mono
    {
       background-image: url("/home/pi/mono.png");
       background-color: transparent;
    }
    .veg
    {
       background-image: url("/home/pi/rig.png");
       background-color: transparent;
    }
    .gri
    {
       background-image: url("/home/pi/gri.png");
       background-color: transparent;
    }
    .igr
    {
       background-image: url("/home/pi/xmas.png");
       background-color: transparent;
    }
    
    .bw
    {
       background-image: url("/home/pi/bw.png");
       background-color: transparent;
    }
    
    .fire *, .hues *, .bleu *, .tiv *, .lava *, .pwr *, .pwrr *, .rainb *
    {
       background-color: transparent;
       color: #000;
    }
    .fever *, .room *, .amb *
    {
       background-color: transparent;
       color: #fff;
    }
    .rgi *, .cir *, .mono *, .veg *
    {
       background-color: transparent;
       color: #000;
    }
    .gri *
    {
       background-color: transparent;
       color: #cfc;
    }
    
    .igr *
    {
       background-color: transparent;
       color: #030;
       font-weight: bold;
    }
    
    .bw *
    {
       background-color: transparent;
       color: #000;
       font-weight: bold;
    }
    
    .tinytxt, .tinytxt *
    {
       font-size: 22px;
    }
    .smtxt, .smtxt *
    {
       font-size: 28px;
    }
    .ifon, .ifon *
    {
       background-color: #9966ff;
       color: #000;
    }
    .vfon, .vfon *
    {
       background-color: #ffff99;
       color: #000;
    }
    
    .rgibig *, .cirbig *, .monbig *, .vegbig *, .rsbig *, .xmsbig *, .ibbig *
    {
       background-color: transparent;
       font-size: 53px;
       font-weight: bold;
       color: #fff;
    }
    
    .monbig *, .vegbig *
    {
        color: #000;
    }
    
    .rgibig
    {
       background-image: url("/home/pi/rgi.png");
       background-size: cover;
       background-color: transparent;
    }
    
    .cirbig
    {
       background-image: url("/home/pi/cir.png");
       background-size: cover;
       background-color: transparent;
    }
    
    .monbig
    {
       background-image: url("/home/pi/mono.png");
       background-size: cover;
       background-color: transparent;
    }
    
    .vegbig
    {
       background-image: url("/home/pi/rig.png");
       background-size: cover;
       background-color: transparent;
    }
    
    .rsbig
    {
       background-image: url("/home/pi/gri.png");
       background-size: cover;
       background-color: transparent;
    }
    
    .xmsbig
    {
       background-image: url("/home/pi/xmas.png");
       background-size: cover;
       background-color: transparent;
    }
    
    .ibbig
    {
       background-image: url("/home/pi/bw.png");
       background-size: cover;
       background-color: transparent;
    }
    
)GLOBCSS";

    
