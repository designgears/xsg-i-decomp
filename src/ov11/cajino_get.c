/*
 * OV11 original TU 2: 0x00a091f8..0x00a09224 (2 functions)
 */
#include "common.h"
#include "shared.h"

typedef struct {
    char *title;
    char *info;
} CajinoText;

CajinoText ViewerText[] = {
    { "\014\273\377\345Shion 1",
      "\014\336\266\377Image illustration by Kunihiko Tanaka. Drawn with\nacrylic on B3 paper. The illustration shows that\nShion's uniform is cream-colored\nwith a tinge of yellow.\n" },
    { "\014\273\377\345Shion 2",
      "\014\336\266\377Full-body design illustrations of Shion. These\nwere done very early in production, so she\ndoesn't have her choker (an accessory worn\naround her neck) yet." },
    { "\014\273\377\345Shion 3",
      "\014\336\266\377Design illustrations of Shion's facial expressions.\nOver 10 versions of Shion were drawn before\nthese were selected. She had been designed\nwith eyeglasses from the beginning, however." },
    { "\014\273\377\345chaos 1",
      "\014\336\266\377Image illustration by Kunihiko Tanaka. The lines\naccentuate the gloves, chaos' trademark." },
    { "\014\273\377\345chaos 2",
      "\014\336\266\377Design illustrations of chaos' facial expressions.\nThe actual polygon model was based on a\nprevious version of this illustration. This latter\nversion emphasizes the flip of his bangs." },
    { "\014\273\377\345chaos 3",
      "\014\336\266\377Full-body design illustrations of chaos. As his\ncharacter is supposed to be a slender young boy,\nhis waist is drawn slim. chaos did not change\nmuch from the initial concept sketch." },
    { "\014\273\377\345Jr. 1",
      "\014\336\266\377Image illustration by Kunihiko Tanaka. He looks a\nlittle younger here than the design illustration." },
    { "\014\273\377\345Jr. 2",
      "\014\336\266\377Design illustrations of Jr.'s facial expressions.\nThe requested image of Jr. was that of a rascal,\nso he is drawn here with a wider range of\nexpressions than in the game." },
    { "\014\273\377\345Jr. 3",
      "\014\336\266\377Full-body design illustrations of Jr., without his\nlong coat. His pants have zippers at the knees\nto detach the ends. Although not drawn here,\nhe has twin holsters on the back of his hips." },
    { "\014\273\377\345Jr. 4",
      "\014\336\266\377Jr. in his long coat. He takes this coat off while\nhe is on an A.G.W.S." },
    { "\014\273\377\345MOMO 1",
      "\014\336\266\377Image illustration by Kunihiko Tanaka. This shows\nthe actual colors used in MOMO's uniform.\nSlightly different from the game, the uniform\ndrawn here uses black as a base color." },
    { "\014\273\377\345MOMO 2",
      "\014\336\266\377Design illustrations of MOMO's facial expressions.\nThe one facing forward is a rare look for MOMO,\nwho in the story often looks melancholy. Her hair\nornament design is from pre-production." },
    { "\014\273\377\345MOMO 3",
      "\014\336\266\377Full-body design illustrations of MOMO. The\ndesign request was for something that\n\042resembled a school uniform.\042 It looks rather\nsimple, but this costume design is intricate." },
    { "\014\273\377\345KOS-MOS 1",
      "\014\336\266\377Image illustration by Kunihiko Tanaka. Her body\ncolor in the game is more of a light blue, but\nthe actual design image uses pearl white as a\nbase color." },
    { "\014\273\377\345KOS-MOS 2",
      "\014\336\266\377Design illustrations of KOS-MOS' facial\nexpressions. These were done early in production,\nas indicated by the visor on her head, which is\ndifferent from the final design." },
    { "\014\273\377\345Ziggy 1",
      "\014\336\266\377Image illustration by Kunihiko Tanaka. Note that\neach image illustration is drawn with equal\nproportions to the actual, scaled down\ncharacters on screen." },
    { "\014\273\377\345Ziggy 2",
      "\014\336\266\377Design illustrations of Ziggy's facial expressions.\nHis body design was not finalized yet when these\nwere done, so his face is drawn slightly slimmer." },
    { "\014\273\377\345Ziggy 3",
      "\014\336\266\377Full-body design illustration of Ziggy. His image\nillustration was created from this picture.\nZiggy's uniform was designed by Tadahiro Usuda." },
    { "\014\273\377\345Gaignun 1",
      "\014\336\266\377Design illustrations for Gaignun's facial\nexpressions. Gaignun's design was developed\nin parallel with Jr." },
    { "\014\273\377\345Gaignun 2",
      "\014\336\266\377Full-body design illustrations for Gaignun. They\nfeature his debonair style." },
    { "\014\273\377\345Elsa",
      "\014\336\266\377Design illustrations of the Elsa. The image of\nthe Elsa is based on a combination of a swan,\na fighter jet, and a race car. The oval parts on\nboth sides contain an \042extraordinary secret.\042\n" },
    { "\014\273\377\345AG-01",
      "\014\336\266\377Early design illustration of an A.G.W.S. by Mr. \nIshigaki. Many versions were made before settling\non this design. The leg joint areas required a lot\nof trial and error due to their unique form." },
    { "\014\273\377\345Cockpit",
      "\014\336\266\377Design illustrations of a cockpit. Although not\nseen here, the interior works like a mold to fit\nthe body contours of the pilot. An EMG signal-\nsensitive gel fills the interior.\n" },
    { "\014\273\377\345VX-9000",
      "\014\336\266\377Design illustration of an A.G.W.S. by Kouichi\nMugitani. This design was done immediately\nfollowing the VX-10000, and the design concept\nwas to use exposed carbon fiber frames." },
    { "\014\273\377\345AG-04",
      "\014\336\266\377Design illustration of an A.G.W.S. by Mr. Ishigaki.\nThis model was designed for hand-to-hand\ncombat, with unique details around the arms and\nlegs. The legs transform for optimized mobility." },
    { "\014\273\377\345VX-20000",
      "\014\336\266\377Design illustrations of an A.G.W.S. by Mr.\nMugitani. The idea behind this design was the\nimage of a concept car." },
    { "\014\273\377\345VX-4000",
      "\014\336\266\377Design illustrations of an A.G.W.S. by Mr.\nMugitani. This was the result of a request to\ncome up with a female-type A.G.W.S., and it was\ndesigned to be used as MOMO's machine." },
    { "\014\273\377\345AG-05",
      "\014\336\266\377Design illustration of an A.G.W.S. by Mr. Ishigaki.\nThis heavy-armored model was designed for\nlong-range fire support. The cannon mounted on\nits back is supported by claws on both arms." },
    { "\014\273\377\345Shion CG",
      "\014\336\266\377CG image of Shion. A high-polygon version of the\n3D model seen in the game, she was created for\na game expo. She was presented at the expo on\na billboard over 3m tall." },
    { "\014\273\377\345KOS-MOS CG",
      "\014\336\266\377CG image of KOS-MOS, also created for a game\nexpo. The gun in her hand is usually stored in a\ncase by her right hip." },
};

char *Cajino_GetTitle(int no)
{
    return ViewerText[no].title;
}

char *Cajino_GetInfo(int no)
{
    return ViewerText[no].info;
}
