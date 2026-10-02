# ------------------------------------------------------------
# Scène Ren'Py : "Un été à Biarritz" — version SSS
# Copie de scene_vacances.rpy, enrichie pour l'exemple renpy_scene :
#   - des sprites (show ... at left/center/right), plusieurs par côté possible
#   - les effets SSS en ligne : **gras**, ~~vague~~, %%tremblement%%
#   - les noms des personnages (Léa, Tom, Camille) sont colorés automatiquement
#     dans le texte, avec la couleur de leur `define`
#   - variables : `default`, `$ var = "..."` puis [var] dans le texte
# ------------------------------------------------------------

define l = Character("Léa", color="#ff7eb6")
define t = Character("Tom", color="#4aa3ff")
define c = Character("Camille", color="#f2b632")

image l = "char/char4.png"
image c = "char/char5.png"
image t = "char/char1.png"
image bg chambre = "char/background.jpg"

default choix_lea = ""

label start:

    scene expression "#f6a55c"   # coucher de soleil

    "La plage se vide doucement. Le ciel devient ~~orange, puis rose.~~"
    "Léa, Tom et Camille sont assis sur le sable, les pieds dans l'eau."
    "C'est le **dernier soir** des vacances."

    show l at left
    show c at right

    c "Bon. Quelqu'un va bien finir par en parler, alors je me lance."
    c "Léa, tu n'as pas dit **un mot** depuis une heure."

    l "Je réfléchis, c'est tout."

    c "Tu réfléchis à un certain ~~Tom~~, peut-être ?"

    show t at left   # derrière Léa : les personnages d'un même côté font la queue

    t "Euh... je suis juste là, hein."

    l "%%Camille !%%"

    c "Quoi ? Depuis **trois semaines** vous ne vous quittez plus. Tout le camping l'a remarqué."

    t "Ah bon ? Je pensais qu'on était discrets."

    c "Tom, vous avez partagé **la même glace** tous les jours."

    t "C'était pour %%économiser%%."

    l "(rires) ~~Menteur.~~"

    scene expression "#e8765f"   # le ciel rougit
    show l at left
    show t at right

    "Un silence s'installe. Les vagues font ~~le reste du bruit~~."

    t "Léa... **moi aussi, ~~j'y pense~~** depuis ce matin."
    t "Demain, je repars à **Lille**. Toi, tu rentres à **Lyon**."

    l "Six cents kilomètres."

    show c at center

    c "%%Sept cents%%, exactement. Je viens de regarder."

    l "Merci, Camille, **très** utile."

    c "Je suis réaliste. Quelqu'un doit l'être dans ce trio."

    t "Alors, **qu'est-ce qu'on fait ?**"

    menu:
        "Un amour de vacances, ça reste un ~~souvenir~~.":
            $ choix_lea = "souvenir"
            jump fin_souvenir

        "Essayons de nous revoir, **quelle que soit la distance**.":
            $ choix_lea = "essayer"
            jump fin_essayer

label fin_souvenir:

    scene expression "#5b4b8a"   # début de nuit
    show l at left
    show t at right

    l "On a passé un été ~~magnifique~~, Tom. Je ne veux pas le gâcher."
    l "Gardons-le comme il est : **parfait**."

    t "(souriant) Tu as sans doute raison."
    t "Mais je n'oublierai pas cette plage."

    show c at center

    c "Bon... je ne pleure pas, c'est %%le sel dans l'air%%."

    hide c
    hide t
    hide l

    "Ils regardent le soleil disparaître, en silence, ensemble une dernière fois."

    "~~FIN~~"
    return

label fin_essayer:

    scene expression "#ff9f7a"   # ciel chaud
    show l at left
    show t at right

    l "Je ne sais pas si ça marchera, mais **j'ai envie d'essayer**."
    l "Donne-moi ton numéro. Le vrai, pas celui du camping."

    t "(riant) Il n'y avait %%pas%% de numéro du camping."

    show c at center

    c "Sept cents kilomètres, vous êtes **courageux**."
    c "Mais je vous accompagne à la gare demain matin. Pour les larmes, je m'en occupe."

    l "Tu es ~~la meilleure~~."

    scene bg chambre

    "Le soleil se couche, mais **quelque chose ~~commence~~**."

    "~~FIN~~"
    return
