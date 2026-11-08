"""Vendor a subset of JulioCacko's Free Input Prompts (CC0) into port/assets/prompts.

usage: python3 port/tools/vendor_prompts.py <extracted pack>/Source
Writes <family>/<style>/<button>.png (96 px) for the buttons the game uses."""
import glob, os, re, sys
from PIL import Image

SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'prompts')
STYLES = {'default': 'Default', 'light': 'Light', 'retro': 'Retro', 'alt': 'Alt', 'alt2': 'Alt 2'}
KB_STYLES = {'default': 'Dark', 'light': 'White', 'retro': 'Retro', 'alt': 'Alt', 'alt2': 'Vintage'}
# button -> candidate stems (first match wins; style suffix and -1 copies ignored)
FAM = {
    'xbox': ('XGamepad', 'T_X_', {'south': ['A_Color', 'A_White'], 'east': ['B_Color', 'B_White'], 'west': ['X_Color', 'X_White'],
                                  'north': ['Y_Color', 'Y_White'], 'lb': ['LB'], 'rb': ['RB'], 'lt': ['LT'], 'rt': ['RT'],
                                  'back': ['Share']}),
    'playstation': ('P5Gamepad', 'T_P5_', {'south': ['Cross_Color', 'Cross'], 'east': ['Circle_Color', 'Circle'],
                                           'west': ['Square_Color', 'Square'], 'north': ['Triangle_Color', 'Triangle'],
                                           'lb': ['L1'], 'rb': ['R1'], 'lt': ['L2'], 'rt': ['R2'], 'back': ['Share'],
                                           'start': ['Options']}),
    'nintendo': ('SGamepad', 'T_S_', {'south': ['B'], 'east': ['A'], 'west': ['Y'], 'north': ['X'], 'lb': ['LB'], 'rb': ['RB'],
                                      'lt': ['LT'], 'rt': ['RT'], 'back': ['Minus'], 'start': ['Plus']}),
    # keyboard: the default binds (xsg_input.cpp DefaultMaps)
    'keyboard': ('Keyboard_Mouse', 'T_', {'south': ['K_Key'], 'east': ['L_Key'], 'west': ['J_Key'], 'north': ['I_Key'],
                                          'lb': ['Q_Key'], 'rb': ['E_Key'], 'lt': ['1_Key'], 'rt': ['3_Key'],
                                          'back': ['BackSpace_Key', 'Backspace_Alt_Key'], 'start': ['Enter_Key']}),
}
n = 0
for fam, (folder, prefix, buttons) in FAM.items():
    for style, sdir in (KB_STYLES if fam == 'keyboard' else STYLES).items():
        files = glob.glob(os.path.join(SRC, folder, sdir, '*.png'))
        if not files:
            files = glob.glob(os.path.join(SRC, folder, 'Default', '*.png'))
        for btn, stems in buttons.items():
            hit = None
            for stem in stems:
                rx = re.compile(re.escape(prefix + stem) + r'(_(Light|Retro|Alt|Alt_2|Dark|White|Vintage|Key_\w+))*\.png$', re.I)
                c = sorted(f for f in files if rx.search(os.path.basename(f)) and '-1' not in os.path.basename(f))
                if c:
                    hit = c[0]
                    break
            if not hit:
                continue
            d = os.path.join(OUT, fam, style)
            os.makedirs(d, exist_ok=True)
            Image.open(hit).convert('RGBA').resize((96, 96), Image.LANCZOS).save(os.path.join(d, btn + '.png'), optimize=True)
            n += 1
with open(os.path.join(OUT, 'LICENSE.txt'), 'w') as f:
    f.write('Input prompt icons: "FREE Input Prompts Pack" by JulioCacko (https://juliocacko.itch.io/free-input-prompts),\n'
            'CC0 1.0 Universal (public domain dedication). Resized subset; vendored by port/tools/vendor_prompts.py.\n')
print(n, 'icons')
