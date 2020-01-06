set -eu

THIS_SCRIPT=$(readlink -m $0)
HERE=$(dirname ${THIS_SCRIPT})
ROOT=$(dirname ${HERE})

while true; do
    echo "Watching for changes..."
    inotifywait -r ${ROOT}/docs/ -q \
		-e modify 			\
		-e close_write 		\
		-e move 			\
		-e delete 			\
		-e create
    make docs || :
done
