// Die Eigenschaftsseite des gewaehlten Segments.
//
// Der eine grosse Fall: `drawTab` mit ueber fuenfhundert Zeilen. Sie zu
// zerschneiden waere moeglich, aber falsch — es ist eine Fallunterscheidung
// ueber die Reiter, und jeder Zweig steht fuer sich. Eine Aufteilung wuerde
// den Zusammenhang zerreissen, den die Reiter gerade ausmachen.
//
// Was sie stattdessen bekommen hat: eine eigene Datei, in der nichts
// anderes steht.
//
// Teil der Klasse App aus app.h.
#include "app.h"

#include "efx/diag.h"
#include "efx/i18n.h"

#include <algorithm>
#include <cstdio>
#include "app_shared.h"

namespace efx::gui {

using i18n::Str;
using i18n::tr;

void App::drawProperties(float width, float height) {
    ImGui::BeginChild("properties", ImVec2(width, height), ImGuiChildFlags_Borders);

    if (doc().selectedPrimitive < 0 ||
        doc().selectedPrimitive >= static_cast<int>(doc().effect.primitives.size())) {
        ImGui::TextDisabled("%s", tr(Str::PropNoSelection));
        ImGui::EndChild();
        return;
    }

    Primitive& p = doc().effect.primitives[doc().selectedPrimitive];

    ImGui::TextUnformatted(typeName(p.type));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", typeDescription(p.type));

    // Felder, die dieser Typ gar nicht ausliest, aber gesetzt sind. Das ist
    // die haeufigste Art, Zeit zu verlieren: man dreht an einer Zahl, die
    // niemand liest, und sucht den Fehler dann woanders.
    const auto useless = fields::uselessFields(p);
    if (!useless.empty()) {
        const theme::Palette& palette = activeTheme(settings_).palette;
        std::string text = tr(Str::FieldNotUsed);
        text += ":";
        for (auto field : useless) {
            text += " ";
            text += fields::fieldName(field);
        }
        ImGui::TextColored(toImGui(palette.warning), "%s", text.c_str());
    }
    ImGui::Separator();

    // Nur die Reiter, die dieser Typ tatsaechlich hat. Die Zuordnung steht in
    // src/fields.cpp und ist aus dem Spielcode hergeleitet, nicht geraten.
    if (ImGui::BeginTabBar("propertyTabs")) {
        for (auto tab : fields::tabsFor(p.type)) {
            if (ImGui::BeginTabItem(tr(fields::tabLabel(tab)))) {
                ImGui::BeginChild("tabbody", ImVec2(0, 0), ImGuiChildFlags_None);
                drawTab(tab, p);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();
}

void App::drawTab(fields::Tab tab, Primitive& p) {
    bool changed = false;

    switch (tab) {
        case fields::Tab::Generation: {
            char nameBuffer[32];
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", p.name.c_str());
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::InputText(tr(Str::FieldName), nameBuffer, sizeof(nameBuffer))) {
                p.name = nameBuffer;
                changed = true;
            }
            // FX_MAX_PRIM_NAME ist 32 einschliesslich Nullbyte — das Feld
            // begrenzt schon, aber der Hinweis erspart die Suche.
            if (p.name.size() >= 31) {
                ImGui::TextDisabled("31");
            }

            // Felder, die fuer diesen Typ nichts bewirken, werden AUSGEGRAUT
            // gezeigt — nicht weggelassen.
            //
            // Das Original macht es so, und es ist die bessere Loesung: ein
            // fehlendes Feld sieht aus wie ein Fehler im Programm, ein
            // ausgegrautes sagt "gibt es, wirkt hier aber nicht". Beim
            // CameraShake ist genau das aufgefallen — "count is missing".
            //
            // Der Kurzhinweis nennt den Grund, den man dem grauen Feld sonst
            // nicht ansieht.
            {
                const bool hasCount = fields::applies(p.type, fields::Field::Count);
                ImGui::BeginDisabled(!hasCount);
                changed |= editRange("count", tr(Str::FieldCount), p.count, 1.0f);
                ImGui::EndDisabled();
                if (!hasCount && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("%s", tr(Str::FieldCountFixed));
                }
            }
            {
                const bool hasLife = fields::applies(p.type, fields::Field::Life);
                ImGui::BeginDisabled(!hasLife);
                changed |= editRange("life", tr(Str::FieldLife), p.life, 10.0f);
                ImGui::EndDisabled();
            }
            changed |= editRange("delay", tr(Str::FieldDelay), p.delay, 10.0f);
            // Gehoert direkt unter die Verzoegerung, nicht in eine Flagliste
            // weiter unten — es beschreibt, wie diese Spanne benutzt wird.
            {
                bool even = (p.spawnFlags & kSpawnEvenDistribution) != 0;
                ImGui::Indent();
                // Ohne Anzahl gibt es nichts zu verteilen — im Original ist
                // das Haekchen dann ebenfalls ausgegraut.
                ImGui::BeginDisabled(!p.delay.set || !p.delay.ranged ||
                                     !fields::applies(p.type, fields::Field::Count));
                if (ImGui::Checkbox(tr(Str::GenEvenDelay), &even)) {
                    p.spawnFlags = even ? (p.spawnFlags | kSpawnEvenDistribution)
                                        : (p.spawnFlags & ~kSpawnEvenDistribution);
                    changed = true;
                }
                ImGui::EndDisabled();
                ImGui::Unindent();
            }

            if (fields::applies(p.type, fields::Field::CullRange)) {
                bool set = p.cullRangeSet;
                if (ImGui::Checkbox("##cullset", &set)) {
                    p.cullRangeSet = set;
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(!p.cullRangeSet);
                ImGui::SetNextItemWidth(146.0f);
                if (ImGui::DragInt(tr(Str::FieldCullRange), &p.cullRange, 10.0f, 0,
                                   100000)) {
                    p.cullRangeSet = true;
                    changed = true;
                }
                ImGui::EndDisabled();
            }

            // "Death Effects" mit "Enable Death Effects" — im Original
            // Dialog 152, nicht bei der Physik.
            if (fields::applies(p.type, fields::Field::DeathFx)) {
                ImGui::SeparatorText(tr(Str::FieldDeathFx));
                bool enabled = (p.flags & kFlagDeathRunsFx) != 0;
                if (ImGui::Checkbox("deathFx", &enabled)) {
                    p.flags = enabled ? (p.flags | kFlagDeathRunsFx)
                                      : (p.flags & ~kFlagDeathRunsFx);
                    changed = true;
                }
                ImGui::BeginDisabled(!enabled);
                changed |= editStringList("deathfx", "", p.deathFx, "");
                ImGui::EndDisabled();
            }

            // Die vollstaendige Flagliste bleibt zusaetzlich. Raven verteilt
            // die Flags auf die Reiter, an die sie gehoeren — das haben wir
            // uebernommen —, aber wer eine fremde Datei oeffnet, will alles
            // an einer Stelle sehen koennen.
            if (ImGui::CollapsingHeader(tr(Str::FlagsGroup))) {
                changed |= editFlags(p);
            }
            break;
        }

        case fields::Tab::OriginSize: {
            // Forward / Right / Up statt X / Y / Z.
            //
            // Der Ursprung wird mit der Effektachse verrechnet:
            //     org = ax[0]*x + ax[1]*y + ax[2]*z
            // ax[0] ist vorwaerts, ax[1] rechts, ax[2] oben. X/Y/Z zu
            // schreiben waere schlicht falsch, solange die Achse nicht der
            // Weltachse entspricht — Raven hat das richtig benannt, ich nicht.
            ImGui::SeparatorText(tr(Str::FieldOrigin));
            changed |= editVec3Range("origin", "", p.origin);
            ImGui::TextDisabled("%s / %s / %s", tr(Str::OriginForward),
                                tr(Str::OriginRight), tr(Str::OriginUp));

            // "Relativ zur Effektachse" ist die Umkehrung von cheapOrgCalc:
            //   if (cheapOrgCalc || relative) -> reine Weltversaetze
            //   sonst                          -> entlang der Effektachse
            bool relativeToAxis = (p.spawnFlags & kSpawnCheapOrgCalc) == 0;
            if (ImGui::Checkbox(tr(Str::OriginRelativeAxis), &relativeToAxis)) {
                p.spawnFlags = relativeToAxis ? (p.spawnFlags & ~kSpawnCheapOrgCalc)
                                              : (p.spawnFlags | kSpawnCheapOrgCalc);
                changed = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tr(Str::OriginAxisHint));

            // Kugel- und Zylinderverteilung. Im Original sind das
            // Auswahlknoepfe neben Radius und Hoehe statt roher Flagnamen —
            // und das ist verstaendlicher, weil man sieht, welche Felder
            // dazugehoeren.
            ImGui::SeparatorText(tr(Str::OriginSpecialOffsets));
            const bool onSphere = (p.spawnFlags & kSpawnOrgOnSphere) != 0;
            const bool onCylinder = (p.spawnFlags & kSpawnOrgOnCylinder) != 0;
            bool special = onSphere || onCylinder;
            if (ImGui::Checkbox("##special", &special)) {
                if (!special) {
                    p.spawnFlags &= ~(kSpawnOrgOnSphere | kSpawnOrgOnCylinder |
                                      kSpawnAxisFromSphere);
                } else {
                    p.spawnFlags |= kSpawnOrgOnSphere;
                }
                changed = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!special);
            int shape = onCylinder ? 1 : 0;
            if (ImGui::RadioButton(tr(Str::OriginSpherical), &shape, 0)) {
                p.spawnFlags |= kSpawnOrgOnSphere;
                p.spawnFlags &= ~kSpawnOrgOnCylinder;
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(tr(Str::OriginCylindrical), &shape, 1)) {
                p.spawnFlags |= kSpawnOrgOnCylinder;
                p.spawnFlags &= ~kSpawnOrgOnSphere;
                changed = true;
            }
            if (fields::applies(p.type, fields::Field::Radius)) {
                changed |= editRange("radius", tr(Str::FieldRadius), p.radius);
            }
            if (fields::applies(p.type, fields::Field::Height)) {
                changed |= editRange("height", tr(Str::FieldHeight), p.height);
            }
            bool axisFromOffset = (p.spawnFlags & kSpawnAxisFromSphere) != 0;
            if (ImGui::Checkbox(tr(Str::OriginAxisFromOffset), &axisFromOffset)) {
                p.spawnFlags = axisFromOffset
                                   ? (p.spawnFlags | kSpawnAxisFromSphere)
                                   : (p.spawnFlags & ~kSpawnAxisFromSphere);
                changed = true;
            }
            ImGui::EndDisabled();

            if (fields::applies(p.type, fields::Field::Size)) {
                ImGui::SeparatorText(tr(Str::FieldSize));
                changed |= editChannel("size", "", p.size);
            }
            // Die Drehung steht im Original in beiden Seiten. Wir zeigen sie
            // nur einmal: hier, wenn es keine Bewegungsseite gibt, sonst dort.
            const auto tabsOfType = fields::tabsFor(p.type);
            const bool hasMotion =
                std::find(tabsOfType.begin(), tabsOfType.end(),
                          fields::Tab::Motion) != tabsOfType.end();
            if (!hasMotion && fields::applies(p.type, fields::Field::Rotation)) {
                ImGui::SeparatorText(tr(Str::FieldRotation));
                changed |= editRange("rotation", "", p.rotation, 0.5f);
                if (fields::applies(p.type, fields::Field::RotationDelta)) {
                    changed |= editRange("rotationDelta",
                                         tr(Str::FieldRotationDelta),
                                         p.rotationDelta, 0.5f);
                }
            }

            // "Always draw on top" — dasselbe Flag, aber dort, wo es wirkt.
            {
                bool onTop = (p.flags & kFlagDepthHack) != 0;
                if (ImGui::Checkbox(tr(Str::OriginDepthHack), &onTop)) {
                    p.flags = onTop ? (p.flags | kFlagDepthHack)
                                    : (p.flags & ~kFlagDepthHack);
                    changed = true;
                }
            }
            break;
        }

        case fields::Tab::Color:
            if (p.type == PrimitiveType::Emitter) {
                ImGui::TextDisabled("%s", tr(Str::ColorEmitterNote));
                ImGui::Separator();
            }
            changed |= editColorChannel(p.rgb);
            if (fields::applies(p.type, fields::Field::Alpha)) {
                changed |= editChannel("alpha", tr(Str::FieldAlpha), p.alpha, 0.01f);
            }
            {
                // "Modulate RGB value using alpha value" — im Original genau
                // hier, weil es beschreibt, wie Farbe und Alpha
                // zusammenwirken. In einer Flagliste weiter unten waere der
                // Zusammenhang nicht zu erkennen.
                bool useAlpha = (p.flags & kFlagUseAlpha) != 0;
                if (ImGui::Checkbox(tr(Str::ColorModulateAlpha), &useAlpha)) {
                    p.flags = useAlpha ? (p.flags | kFlagUseAlpha)
                                       : (p.flags & ~kFlagUseAlpha);
                    changed = true;
                }
            }
            if (fields::applies(p.type, fields::Field::Shaders)) {
                ImGui::SeparatorText(tr(Str::FieldShaders));
                changed |= editStringList("shaders", "", p.shaders, "gfx/");

                // Wie jeder Shader tatsaechlich aufgeloest wurde.
                //
                // Warum das noetig ist: ob ein Feuer einen hellen Kern
                // bekommt, haengt an der Mischung. ADDITIV addieren sich
                // uebereinanderliegende Sprites zu Weiss auf — das ist der
                // Kern. ALPHAGEMISCHT deckt jedes das darunter nur ab, und
                // der Kern bleibt aus.
                //
                // Steht der Name in keiner .shader-Datei, baut die Engine
                // einen Ersatz. `RE_RegisterShader` meldet ihn mit
                // `lightmaps2d` an, und R_FindShader nimmt dafuer
                //
                //     GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA
                //
                // also Alphamischung. Ein Feuer, dessen Shader nicht gefunden
                // wird, sieht deshalb flau aus — und man sucht den Fehler an
                // der falschen Stelle, naemlich beim Effekt statt beim Pfad.
                //
                // Die Zeile sagt beides: gefunden oder Ersatz, und welche
                // Mischung daraus folgt.
                for (const std::string& name : p.shaders) {
                    const bool known = assets_.hasShader(name);
                    const shader::BlendMode mode = assets_.blendOf(name);
                    const char* modeText =
                        mode == shader::BlendMode::Additive    ? tr(Str::BlendAdditive)
                        : mode == shader::BlendMode::AlphaBlend ? tr(Str::BlendAlpha)
                                                                : tr(Str::BlendOpaque);
                    const theme::Palette& pal = activeTheme(settings_).palette;
                    if (known) {
                        ImGui::TextColored(toImGui(pal.textDim), "%s: %s",
                                           name.c_str(), modeText);
                    } else {
                        ImGui::TextColored(toImGui(pal.warning), "%s: %s (%s)",
                                           name.c_str(), modeText,
                                           tr(Str::ShaderFallback));
                    }
                }
                bool shaderTime = (p.flags & kFlagSetShaderTime) != 0;
                if (ImGui::Checkbox(tr(Str::ColorShaderTime), &shaderTime)) {
                    p.flags = shaderTime ? (p.flags | kFlagSetShaderTime)
                                         : (p.flags & ~kFlagSetShaderTime);
                    changed = true;
                }
            }
            break;

        case fields::Tab::Motion: {
            // Aufbau wie Dialog 160, gemessen am Emitter.
            ImGui::SeparatorText(tr(Str::FieldVelocity));
            changed |= editVec3Range("velocity", "", p.velocity);
            ImGui::TextDisabled("%s / %s / %s", tr(Str::OriginForward),
                                tr(Str::OriginRight), tr(Str::OriginUp));
            {
                // "Relativ zur Effektachse" ist die Umkehrung von absoluteVel.
                bool relative = (p.spawnFlags & kSpawnVelIsAbsolute) == 0;
                if (ImGui::Checkbox(tr(Str::MotionRelativeAxis), &relative)) {
                    p.spawnFlags = relative ? (p.spawnFlags & ~kSpawnVelIsAbsolute)
                                            : (p.spawnFlags | kSpawnVelIsAbsolute);
                    changed = true;
                }
                // Wind: Haekchen und Anteil gehoeren zusammen. Das Flag kennt
                // nur der Multiplayer-Zweig — das steht im Flagnamen selbst,
                // und die Pruefung meldet es beim SP-Ziel.
                bool wind = (p.spawnFlags & kSpawnAffectedByWind) != 0;
                if (ImGui::Checkbox(tr(Str::MotionAffectedByWind), &wind)) {
                    p.spawnFlags = wind ? (p.spawnFlags | kSpawnAffectedByWind)
                                        : (p.spawnFlags & ~kSpawnAffectedByWind);
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", dialectName(Dialect::MP));
                ImGui::BeginDisabled(!wind);
                ImGui::Indent();
                changed |= editRange("wind", "", p.windModifier, 0.5f);
                ImGui::TextDisabled("%s", tr(Str::MotionWindPercent));
                ImGui::Unindent();
                ImGui::EndDisabled();
            }

            ImGui::SeparatorText(tr(Str::FieldAcceleration));
            changed |= editVec3Range("accel", "", p.acceleration);
            {
                bool relative = (p.spawnFlags & kSpawnAccelIsAbsolute) == 0;
                if (ImGui::Checkbox(tr(Str::MotionRelativeAxis), &relative)) {
                    p.spawnFlags = relative ? (p.spawnFlags & ~kSpawnAccelIsAbsolute)
                                            : (p.spawnFlags | kSpawnAccelIsAbsolute);
                    changed = true;
                }
            }

            changed |= editRange("gravity", tr(Str::FieldGravity), p.gravity, 10.0f);
            ImGui::TextDisabled("%s", tr(Str::MotionGravityHint));

            if (fields::applies(p.type, fields::Field::Rotation)) {
                ImGui::SeparatorText(tr(Str::FieldRotation));
                changed |= editRange("rotation", "", p.rotation, 0.5f);
                if (fields::applies(p.type, fields::Field::RotationDelta)) {
                    changed |= editRange("rotationDelta",
                                         tr(Str::FieldRotationDelta),
                                         p.rotationDelta, 0.5f);
                }
            }
            break;
        }

        case fields::Tab::Physics: {
            // Aufbau wie Dialog 161: Physik einschalten, Rueckprall,
            // Aufprall-Gruppe, Begrenzungsbox, teure Physik. Schwerkraft und
            // Wind gehoeren zur Bewegung, Dichte und Streuung zum Emitter —
            // ich hatte sie hier.
            bool physics = (p.flags & kFlagApplyPhysics) != 0;
            if (ImGui::Checkbox("usePhysics", &physics)) {
                p.flags = physics ? (p.flags | kFlagApplyPhysics)
                                  : (p.flags & ~kFlagApplyPhysics);
                changed = true;
            }
            ImGui::BeginDisabled(!physics);
            // Direkt unter "Enable Physics", wie im Original — nicht am Ende.
            // Es beschreibt, *wie* die Physik rechnet, gehoert also neben den
            // Schalter, der sie einschaltet.
            {
                bool expensive = (p.flags & kFlagExpensivePhysics) != 0;
                ImGui::Indent();
                if (ImGui::Checkbox(tr(Str::PhysicsExpensive), &expensive)) {
                    p.flags = expensive ? (p.flags | kFlagExpensivePhysics)
                                        : (p.flags & ~kFlagExpensivePhysics);
                    changed = true;
                }
                ImGui::Unindent();
            }
            changed |= editRange("bounce", tr(Str::FieldBounce), p.elasticity, 0.05f);

            ImGui::SeparatorText(tr(Str::PhysicsImpacts));
            bool kill = (p.flags & kFlagKillOnImpact) != 0;
            if (ImGui::Checkbox(tr(Str::PhysicsKillOnImpact), &kill)) {
                p.flags = kill ? (p.flags | kFlagKillOnImpact)
                               : (p.flags & ~kFlagKillOnImpact);
                changed = true;
            }
            bool impactPlay = (p.flags & kFlagImpactRunsFx) != 0;
            if (ImGui::Checkbox(tr(Str::PhysicsPlayOnImpact), &impactPlay)) {
                p.flags = impactPlay ? (p.flags | kFlagImpactRunsFx)
                                     : (p.flags & ~kFlagImpactRunsFx);
                changed = true;
            }
            ImGui::BeginDisabled(!impactPlay);
            changed |= editStringList("impactfx", "", p.impactFx, "");
            ImGui::EndDisabled();

            // Die Begrenzungsbox: min und max, und das Flag, das sie erst
            // wirksam macht. Im Original stehen sie in einem Kasten zusammen.
            ImGui::SeparatorText(tr(Str::PhysicsBoundingBox));
            bool bbox = (p.flags & kFlagUseBBox) != 0;
            if (ImGui::Checkbox("useBBox", &bbox)) {
                p.flags = bbox ? (p.flags | kFlagUseBBox) : (p.flags & ~kFlagUseBBox);
                changed = true;
            }
            ImGui::BeginDisabled(!bbox);
            changed |= editVec3Range("min", tr(Str::FieldMin), p.min);
            changed |= editVec3Range("max", tr(Str::FieldMax), p.max);
            ImGui::EndDisabled();

            ImGui::EndDisabled();
            break;
        }

        case fields::Tab::Line: {
            // Drei Auswahlknoepfe statt zweier Flags in einer Liste: der
            // Endpunkt ist entweder angegeben, getract oder ein Versatz.
            // Genau so steht es in Dialog 172.
            // Zwei Auswahlknoepfe, und "als Versatz benutzen" haengt am
            // zweiten. Ich hatte drei gleichrangige Knoepfe — im Original ist
            // der dritte ein eingerueckter Zusatz zum Trace, und das stimmt
            // auch mit der Engine ueberein: org2isOffset veraendert, wie der
            // Trace-Endpunkt gestreut wird.
            int endpointMode = (p.spawnFlags & kSpawnOrg2FromTrace) ? 1 : 0;
            if (ImGui::RadioButton(tr(Str::LineEndpointGiven), &endpointMode, 0)) {
                p.spawnFlags &= ~(kSpawnOrg2FromTrace | kSpawnOrg2IsOffset);
                changed = true;
            }
            if (ImGui::RadioButton(tr(Str::LineEndpointTrace), &endpointMode, 1)) {
                p.spawnFlags |= kSpawnOrg2FromTrace;
                changed = true;
            }
            ImGui::Indent();
            ImGui::BeginDisabled(endpointMode != 1);
            bool asOffset = (p.spawnFlags & kSpawnOrg2IsOffset) != 0;
            if (ImGui::Checkbox(tr(Str::LineEndpointOffset), &asOffset)) {
                p.spawnFlags = asOffset ? (p.spawnFlags | kSpawnOrg2IsOffset)
                                        : (p.spawnFlags & ~kSpawnOrg2IsOffset);
                changed = true;
            }
            ImGui::EndDisabled();
            ImGui::Unindent();

            ImGui::SeparatorText(tr(Str::FieldOrigin2));
            changed |= editVec3Range("origin2", "", p.origin2);
            ImGui::TextDisabled("%s / %s / %s", tr(Str::OriginForward),
                                tr(Str::OriginRight), tr(Str::OriginUp));
            {
                // Eigenes "Relativ zur Effektachse" fuer den Endpunkt:
                // cheapOrg2Calc, nicht cheapOrgCalc.
                bool relative2 = (p.spawnFlags & kSpawnCheapOrg2Calc) == 0;
                if (ImGui::Checkbox(tr(Str::OriginRelativeAxis), &relative2)) {
                    p.spawnFlags = relative2 ? (p.spawnFlags & ~kSpawnCheapOrg2Calc)
                                             : (p.spawnFlags | kSpawnCheapOrg2Calc);
                    changed = true;
                }
            }

            ImGui::SeparatorText(tr(Str::LineEndpointEffects));
            bool traceFx = (p.spawnFlags & kSpawnTraceImpactFx) != 0;
            if (ImGui::Checkbox(tr(Str::LinePlayAtEndpoint), &traceFx)) {
                p.spawnFlags = traceFx ? (p.spawnFlags | kSpawnTraceImpactFx)
                                       : (p.spawnFlags & ~kSpawnTraceImpactFx);
                changed = true;
            }
            ImGui::BeginDisabled(!traceFx);
            changed |= editStringList("impactfx", "", p.impactFx, "");
            ImGui::EndDisabled();

            if (p.type == PrimitiveType::Electricity) {
                // Dialog 172 nennt variance hier "Chaos" — dasselbe Feld,
                // aber der Name sagt endlich, was es tut.
                ImGui::SeparatorText(tr(Str::GroupElectricity));
                changed |= editRange("chaos", tr(Str::LineChaos), p.variance, 0.05f);

                // Verjuengung, Verzweigung, Wachsen. Diese drei teilen sich
                // ihre Bits mit useModel, usePhysics und useBBox — der Parser
                // kennt keine eigenen Schluessel dafuer. In der Datei steht
                // also "flags useModel usePhysics useBBox".
                struct ElectricityFlag { Str label; uint32_t bits; };
                const ElectricityFlag electricityFlags[] = {
                    {Str::LineTaper, kFlagElectricityTaper},
                    {Str::LineBranch, kFlagElectricityBranch},
                    {Str::LineGrow, kFlagElectricityGrow},
                };
                for (const auto& entry : electricityFlags) {
                    bool on = (p.flags & entry.bits) != 0;
                    if (ImGui::Checkbox(tr(entry.label), &on)) {
                        p.flags = on ? (p.flags | entry.bits)
                                     : (p.flags & ~entry.bits);
                        changed = true;
                    }
                }
                ImGui::TextDisabled("%s", tr(Str::LineSharedBits));
            }
            break;
        }

        case fields::Tab::Tail:
        case fields::Tab::LengthSize2:
            changed |= editChannel("length", tr(Str::FieldLength), p.length);
            if (fields::applies(p.type, fields::Field::Size2)) {
                changed |= editChannel("size2", tr(Str::FieldSize2), p.size2);
            }
            break;

        case fields::Tab::Model: {
            // Winkel und Winkeldifferenz gehoeren hierher, nicht zur Bewegung:
            // sie richten das angehaengte Modell aus. Im Original heissen sie
            // Pitch/Yaw/Roll, und das ist die richtige Benennung fuer Winkel
            // in Quake-Reihenfolge.
            ImGui::SeparatorText(tr(Str::FieldAngles));
            changed |= editVec3Range("angles", "", p.angles, 0.5f);
            ImGui::TextDisabled("%s / %s / %s", tr(Str::AnglePitch),
                                tr(Str::AngleYaw), tr(Str::AngleRoll));
            ImGui::SeparatorText(tr(Str::FieldAngleDelta));
            changed |= editVec3Range("angleDelta", "", p.anglesDelta, 0.5f);

            ImGui::SeparatorText(tr(Str::FieldModels));
            bool attach = (p.flags & kFlagAttachedModel) != 0;
            if (ImGui::Checkbox(tr(Str::ModelAttach), &attach)) {
                p.flags = attach ? (p.flags | kFlagAttachedModel)
                                 : (p.flags & ~kFlagAttachedModel);
                changed = true;
            }
            changed |= editStringList("models", "", p.models, "models/");
            break;
        }

        case fields::Tab::Emitter: {
            bool emit = (p.flags & kFlagEmitFx) != 0;
            if (ImGui::Checkbox(tr(Str::EmitterEnable), &emit)) {
                p.flags = emit ? (p.flags | kFlagEmitFx) : (p.flags & ~kFlagEmitFx);
                changed = true;
            }
            ImGui::SeparatorText(tr(Str::FieldEmitFx));
            changed |= editStringList("emitfx", "", p.emitFx, "");
            changed |= editRange("density", tr(Str::FieldDensity), p.density);
            changed |= editRange("variance", tr(Str::FieldVariance), p.variance,
                                 0.05f);
            break;
        }

        case fields::Tab::Sound: {
            changed |= editStringList("sounds", tr(Str::FieldSounds), p.sounds,
                                      "sound/");

            // Anhoeren, ohne den ganzen Effekt abzuspielen.
            //
            // Im Original steht dafuer ein Lautsprecher neben der Liste, und
            // er fehlte uns. Der Unterschied ist groesser, als er klingt: um
            // einen Klang zu pruefen, musste man bisher den kompletten Effekt
            // starten und den richtigen Augenblick abwarten.
            //
            // Ausgegraut, wenn die Liste leer ist — ein Knopf, der nichts tun
            // kann, soll auch nicht so aussehen, als koennte er es.
            // Eine Klangdatei aussuchen, statt den Pfad zu tippen.
            //
            // Im Original steht dafuer ein Ordnersymbol neben der Liste, mit
            // dem Filter "Sound Files (*.wav; *.mp3)". Beides uebernommen —
            // die Endungen sind genau die, die die Engine liest.
            if (ImGui::Button(tr(Str::SoundBrowse)) && fileDialog_) {
                const std::string picked = fileDialog_(
                    false, "Sound files (*.wav; *.mp3)\0*.wav;*.mp3\0"
                           "All files\0*.*\0",
                    settings_.gamePath.c_str());
                if (!picked.empty()) {
                    // Auf den spielinternen Namen kuerzen: in der .efx steht
                    // `sound/...`, kein Laufwerksbuchstabe. Wer den vollen
                    // Pfad eintraegt, bekommt eine Datei, die im Editor
                    // klingt und im Spiel fehlt.
                    const std::string relative = toGameRelative(picked);
                    if (relative.empty()) {
                        diag::warn("sound outside game path: " + picked);
                    } else {
                        p.sounds.push_back(relative);
                        changed = true;
                    }
                }
            }
            ImGui::SameLine();

            const bool empty = p.sounds.empty();
            if (empty) ImGui::BeginDisabled();
            if (ImGui::Button(tr(Str::SoundPlay))) playSoundNow(p.sounds.front());
            if (empty) ImGui::EndDisabled();

            if (empty) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", tr(Str::SoundPlayNone));
            } else if (!settings_.playSounds) {
                // Der haeufigste Grund fuer "da kommt nichts": der Haken im
                // Menue. Danach sucht man lange, wenn es niemand sagt.
                ImGui::SameLine();
                ImGui::TextDisabled("%s", tr(Str::SoundPlayOff));
            }
            break;
        }

        case fields::Tab::FxRunner: {
            bool randomRot = (p.spawnFlags & kSpawnRandRotAroundFwd) != 0;
            if (ImGui::Checkbox(tr(Str::FxRunnerRandomRotation), &randomRot)) {
                p.spawnFlags = randomRot
                                   ? (p.spawnFlags | kSpawnRandRotAroundFwd)
                                   : (p.spawnFlags & ~kSpawnRandRotAroundFwd);
                changed = true;
            }
            ImGui::SeparatorText(tr(Str::FieldPlayFx));
            changed |= editStringList("playfx", "", p.playFx, "");
            break;
        }

        case fields::Tab::CameraShake: {
            // Hier heisst bounce "Staerke". Dieselbe Zahl, andere Bedeutung —
            // die Doppelbelegung erklaert Ravens Editor nirgends, aber er
            // beschriftet wenigstens den Regler: "Nothing" unten, "Quake" und
            // "Insane" oben. Das ist besser als eine nackte Zahl, weil 16 fuer
            // sich genommen nichts sagt.
            changed |= editRange("radius", tr(Str::FieldRadius), p.radius, 5.0f);

            ImGui::SeparatorText(tr(Str::ShakeIntensity));
            bool present = p.elasticity.set;
            if (ImGui::Checkbox("##shakeSet", &present)) {
                p.elasticity.set = present;
                changed = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!p.elasticity.set);
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat("##shakeMin", &p.elasticity.min, 0.0f,
                                   camera::Shake::kMaxIntensity, "%.2f")) {
                if (!p.elasticity.ranged) p.elasticity.max = p.elasticity.min;
                p.elasticity.set = true;
                changed = true;
            }
            if (p.elasticity.ranged) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::SliderFloat("##shakeMax", &p.elasticity.max, 0.0f,
                                       camera::Shake::kMaxIntensity, "%.2f")) {
                    p.elasticity.set = true;
                    changed = true;
                }
            }
            ImGui::SameLine();
            bool ranged = p.elasticity.ranged;
            if (ImGui::Checkbox("~", &ranged)) {
                p.elasticity.ranged = ranged;
                if (!ranged) p.elasticity.max = p.elasticity.min;
                changed = true;
            }
            // Die Landmarken. MAX_SHAKE_INTENSITY ist 16; darueber wirkt
            // nichts mehr, deshalb endet der Regler dort.
            ImGui::Indent();
            ImGui::TextDisabled("0 = %s     8 = %s     16 = %s",
                                tr(Str::ShakeNothing), tr(Str::ShakeQuake),
                                tr(Str::ShakeInsane));
            ImGui::Unindent();
            ImGui::EndDisabled();
            break;
        }
    }

    if (changed) {
        doc().dirty = true;
        refreshDiagnostics();
    }
}


}  // namespace efx::gui
