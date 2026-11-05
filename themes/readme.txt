EmuCenter THEMES
======
themes/all -> COMPLETE themes: change everything (colors, game lists, groups, separators, gradients, logo color) using a :root { --variable: value; } block and may also include button styling rules.

themes/normal -> BUTTON, FONT, and detail packages (Qt Style Sheets, similar to CSS). They are applied on top of the selected theme and work alongside "all" themes.

If a normal theme file contains a :root block, EmuCenter moves that section to themes/all.

:root VARIABLES --bg --panel --panel2 --border --text --muted --accent --group-bg --group-text --row-light --row-light-text --row-dark --row-dark-text --row-selected --row-selected-text --row-separator --row-radius --row-gap --ok --warn --err --plate --logo-color (or "original") --gradient (on|off) --gradient-a --gradient-b

Application order: base theme < all theme < normal package.

See theme-all.css (all) and the normal examples.