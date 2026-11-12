#!/bin/sh
# Rendu des six vues PlantUML de Q6 (classes en 2 volets, sequence en 4 volets).
# Moteur, par ordre de preference :
#   1. commande plantuml du PATH ;
#   2. jar PlantUML designe par PLANTUML_JAR, avec java ;
#   3. repli Docker (image ghcr.io/plantuml/plantuml), seulement si le daemon est accessible.
# Le mode fichiers evite le -pipe du Makefile, qui ne rend pas les vues nommees.
# Le rendu passe par un dossier temporaire ; les PNG existants ne sont remplaces
# qu'apres la reussite complete du rendu. Aucune source n'est modifiee.
set -eu
q6_diagram_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
q6_render_dir=$(mktemp -d)
trap 'rm -rf "$q6_render_dir"' 0
trap 'exit 1' HUP INT TERM
q6_sources='multitaskingAccumulatorClassDiagram.puml multitaskingAccumulatorSequenceDiagram.puml'
q6_views='ClassDiagram ClassInterfaces SequenceAcquisition SequenceProcessing SequenceDiagnostic SequenceLifecycle'
cp "$q6_diagram_dir/multitaskingAccumulatorClassDiagram.puml" \
   "$q6_diagram_dir/multitaskingAccumulatorSequenceDiagram.puml" "$q6_render_dir/"

q6_engine=''
if command -v plantuml >/dev/null 2>&1; then
  q6_engine=local
elif [ -n "${PLANTUML_JAR:-}" ] && [ -f "${PLANTUML_JAR:-}" ] && command -v java >/dev/null 2>&1; then
  q6_engine=jar
elif command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
  q6_engine=docker
else
  printf '%s\n' 'Q6: aucun moteur PlantUML disponible. Installer plantuml, ou definir PLANTUML_JAR (jar PlantUML, avec java), ou rendre Docker accessible.' >&2
  exit 2
fi

cd "$q6_render_dir"
case "$q6_engine" in
  local)
    plantuml -tpng $q6_sources
    ;;
  jar)
    # java.io.tmpdir est place dans le dossier de rendu, accessible en ecriture.
    java -Djava.awt.headless=true -Djava.io.tmpdir="$q6_render_dir" \
      -jar "$PLANTUML_JAR" -tpng $q6_sources
    ;;
  docker)
    docker run --rm --network none --user "$(id -u):$(id -g)" \
      -v "$q6_render_dir:/work" -w /work ghcr.io/plantuml/plantuml \
      --check-before-run --png $q6_sources
    ;;
esac
cd "$q6_diagram_dir"

for q6_name in $q6_views; do
  test -s "$q6_render_dir/multitaskingAccumulator$q6_name.png"
done
# Remplacement seulement apres le succes complet du rendu.
for q6_name in $q6_views; do
  cp "$q6_render_dir/multitaskingAccumulator$q6_name.png" "$q6_diagram_dir/"
done
printf 'Q6: 6 PNG generes avec le moteur %s (classes en 2 volets, sequence en 4 volets).\n' "$q6_engine"
