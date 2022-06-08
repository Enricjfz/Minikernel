# Minikernel
Repositorio del proyecto del minikernel de la asignatura de Sistemas Operativos Avanzados 

Para definir un Mutex, se ha definido como un struct con atributos tales como su nombre, tipo, id del proceso al que pertenece (>=0 si esta siendo usado, -1 si 
no lo esta siendo usado por ningún proceso) y un vector de procesos que estan bloqueados por ese mutex.
Para el proceso, se ha añadido al struct la información de ROUND ROBIN e INTERRUPCIONES para el sleep además de un vector de mutex abiertos por ese proceso que junto 
con el vector de mutex creados, nos permite obtener la información entre procesos<-->mutex.
