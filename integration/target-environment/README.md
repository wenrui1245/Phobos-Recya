# Target environment regression

Targeting distinguishes native underground and airborne layers from explicitly reported physical water depth. Mouse acquisition, automatic acquisition, weapon selection and final AU/AA checks use the shared classifier. Rendering layers, coordinates, underground tracking and save layouts remain unchanged.

The optional depth provider is the existing Kaleidoscope public query. Missing modules or exports retain the previous host rules. Native underground/air layers take precedence over inconsistent depth reports; water depth alone never grants a weapon, enemy eligibility, vision, armor eligibility or range.

`run.ps1` accepts Python, OutDir, PrivacyChecker and optional SourceRoot. Run it in an x86 MSVC environment. It extracts the actual classifier, existing depth consumer and complete mouse/AI/fire callbacks into a private fixture, then checks 1,400 combinations in each of normal, missing-module and missing-export processes. Tests cover AU/AA, shallow/deep/surface/negative heights, native air/underground media, fallback, and lazy SpecialTracked lookup. These tests do not launch the game or establish gameplay compatibility.
# Weapon policy extension

Environment service version 2 additionally queries the optional public
`Kaleidoscope_GetUnderwaterWeaponPolicy(WeaponTypeClass*)` service. A missing
provider or -1 inherits original eligibility; 0 excludes physically submerged
targets; 1 continues original checks. Detection is separate. Grounded/surfaced,
airborne and native Underground retain their old rules, including AU/AA.

Candidate exclusion runs in primary/secondary and gattling selection and the
multi-weapon predicate. Cursor and final CanFire reject the same explicit 0.
This does not make hidden targets visible, change splash collection or bypass
weapon house, armor, range or shield checks. Automatic target acquisition keeps
its native weapon selection and CanFire pipeline without another scan/hook.

The fixture now runs 4200 production classifier/cursor/AI/fire cases for each
of four provider modes, plus the extracted production weapon-picker prelude
for no/one/two slots and every inherit/no/yes combination. The continuation
marker verifies the ordinary two-weapon picking logic remains in use. These
isolated tests do not establish game, driver or multiplayer behavior.
