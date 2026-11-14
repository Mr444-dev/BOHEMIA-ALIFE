# Research basis and model boundaries

This project uses published work as architectural context. The mechanics and data below are an independent implementation; citations do not imply endorsement or access to proprietary studio systems.

## Open-world ambient AI

Plch, Marko, Ondráček, Černý, Gemrot, and Brom describe a production-oriented AI system for a large open virtual world. Its focus is ambient day-to-day behavior, handling hundreds of NPCs, hierarchical control inspired by subsumption, and a visual agent-based language inspired by behavior trees. The standalone core borrows those architectural ideas: layered behavior arbitration, environment-owned behavior fragments, and bounded simulation work. It does not reproduce the original proprietary game implementation.

**Primary citation:** Tomáš Plch, Matěj Marko, Petr Ondráček, Martin Černý, Jakub Gemrot, and Cyril Brom. “An AI System for Large Open Virtual World.” *Proceedings of the AAAI Conference on Artificial Intelligence and Interactive Digital Entertainment*, 10(1), 44–51 (2014). [DOI 10.1609/aiide.v10i1.12705](https://doi.org/10.1609/aiide.v10i1.12705). [AAAI paper page](https://ojs.aaai.org/index.php/AIIDE/article/view/12705).

The paper discusses hundreds of NPCs. The 1,000-NPC target and its 56/148/796 tier split here are a project scaling exercise, not a result reported in that paper.

## Behavior objects and context-driven behavior

Černý et al. introduce behavior objects as an abstraction for encapsulating related behavior and data in the environment, extending smart-object ideas. The project uses that as the basis for five small, fixed-capacity concepts: station SmartObjects, NavigationSmartObjects, SmartAreas, QuestSmartObjects, and coordinated Situations. The particular handle layout, cleanup tree, role solver, and Pepa/Franta fixture are implementation choices in this repository.

**Primary citation:** Martin Černý, Tomáš Plch, Matěj Marko, Jakub Gemrot, Petr Ondráček, and Cyril Brom. “Using Behavior Objects to Manage Complexity in Virtual Worlds.” arXiv:1508.00377 (submitted 2015; related journal DOI 10.1109/TCIAIG.2016.2528499). [arXiv record](https://arxiv.org/abs/1508.00377).

A related 2014 paper on constraint-satisfaction-driven NPC situations is Martin Černý, Cyril Brom, Roman Barták, and Martin Antoš, “Spice It Up! Enriching Open World NPC Simulation Using Constraint Satisfaction,” AIIDE 10(1), 16–22 (2014), [DOI 10.1609/aiide.v10i1.12715](https://doi.org/10.1609/aiide.v10i1.12715). The “Pepa and Franta” example in this repository is a test fixture illustrating a minimum-cardinality constraint; it is not presented as a quote or verified interview from the cited papers.

## Armor model: historical source versus gameplay tuning

Alan Williams' *The Knight and the Blast Furnace* is a historical and metallurgical study that analyzes surviving armor and discusses mechanical testing. This project does not claim that Williams proposed a four-layer Plate → Maille → Gambeson → Stamina equation, the 185 J threshold, the glancing equation, the durability multiplier, or any quoted 115 J / 58° outcome. Those are explicit gameplay calibration values from the project brief. The code uses condition-dependent resistance, a bounded glancing retention rule, blunt-energy transfer, exponential pad attenuation, and stamina-first absorption to create deterministic and inspectable combat behavior.

**Bibliographic citation:** Alan Williams. *The Knight and the Blast Furnace: A History of the Metallurgy of Armour in the Middle Ages and the Early Modern Period*. History of Warfare, vol. 12. Leiden: Brill, 2003. ISBN 978-90-04-12498-1. [Brill book record](https://brill.com/display/title/17861); [Rijksmuseum catalog entry](https://www.rijksmuseum.nl/en/collection/publication/The-knight-and-the-blast-furnace-a-history-of-the-metallurgy-of-armour-in-the-Middle-Ages-and-the-early-modern-period--ec51ad50c119fbf128d1b9e5b9e7e6e2).

## Citation corrections from the supplied brief

- The Plch et al. AIIDE paper is correctly identified by DOI `10.1609/aiide.v10i1.12705`.
- The behavior-objects work is arXiv `1508.00377`, with related IEEE DOI `10.1109/TCIAIG.2016.2528499`. It is not arXiv `1508.01700`.
- arXiv `1508.01700` is a paper on ammonia and CCS observations in infrared dark clouds, unrelated to game AI.
- DOI `10.1609/aiide.v10i1.12715` identifies the separate “Spice It Up!” constraint-satisfaction paper by Černý, Brom, Barták, and Antoš.
