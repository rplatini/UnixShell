# Lab: shell

### Búsqueda en $PATH

**Diferencias entre syscall execve(2) y familia de wrappers de la liberia estandar de C exec(3)**

La syscall execve(2) ejecuta el programa cuyo path esta indicado en el parametro de la funcion. Al ejecutar esta syscall, el programa que esta corriendo en ese proceso es reemplazado por otro programa, con otras direcciones de memoria (nuevo stack, heap, etc). No se crea un nuevo proceso, sino que este se 'transforma' en otro.

La familia de funciones exec() reemplaza la imagen del proceso actual con una nueva imagen, la del nuevo proceso a ejecutarse. Internamente llaman a la syscall execve(2), con el beneficio de que los wrappers difieren en la forma de pasar los argumentos (por ejemplo, si le pasamos el path al binario o directamente el archivo, si podemos modificar el entorno, etc).

La llamada a exec(3) puede fallar. Algunos motivos de falla pueden ser que el archivo/path al binario no se encuentre o que no se tengan los permisos para abrirse. Si ocurre un error, la funcion exec(3) retorna -1. 

En este caso, la implementacion de la shell finaliza su ejecucion retornando un error y realizando un _exit(-1).

---

### Comandos built-in

Entre cd y pwd, *pwd* podria implementarse sin ser un built-in, ya que su implementacion no depende de si es realizada en el proceso de la shell o en su proceso hijo. Este built-in no debe modificar nada en el proceso de la shell, sino que imprime el path del directorio actual. 

El motivo de que pwd sea un built-in es porque es mas eficiente. Si bien podria ser un comando ejecutable, esto implicaria un parseo del comando, luego hacer un fork, ejecutarlo, y que la shell espere a que el proceso termine. En cambio, al ser un built-in, nada de eso es necesario ya que antes de hacer un fork, se evalua si el comando ingresado es *pwd*, y si es asi, se implementa la ejecucion en el proceso de la shell.

---

### Variables de entorno adicionales

Al setear las variables de entorno temporales, es necesario realizarlo luego de hacer el fork(). Esto se debe a que al ser variables temporales, no deben modificar las variables de entorno de la shell. Si las setearamos antes del fork(), estas se crearian/modificarian en el proceso shell (proceso padre), modificando permanentemente las variables. Si las seteamos luego del fork(), las variables se crean en el entorno del proceso hijo, lo cual solo afecta al proceso que la shell va a ejecutar en ese momento.


Si en vez de utilizar setenv(3) utilizaramos uno de los wrappers de la familia de funciones exec(3) que admiten un arreglo de variables de entorno, no obtendriamos el mismo resultado. Al realizar la segunda opcion, estariamos seteando en el nuevo proceso SOLO las variables contenidas en el array envp, mientras que utilizando setenv(3) estamos agregando las variables contenidas en eargv[] a la lista de variables de entorno existentes de ese proceso.

Una posible implementacion para que el comportamiento sea el mismo, seria incluir en el arreglo envp todas las variables de entorno del proceso y ademas las que queremos agregar, y luego llamar a un wrapper de la familia exec(3) pasandole ese arreglo como tercer parametro.


---

### Procesos en segundo plano

Para implementar procesos en segundo plano, el proceso padre no debe bloquear la ejecucion del programa aunque su hijo (el proceso en segundo plano) no haya finalizado. De lo contrario, no se podrian ejecutar otros procesos mientras el que esta corriendo en segundo plano no haya finalizado. Si bien el proceso padre no debe bloquear el flujo de ejecucion, debe esperar a que su hijo termine antes de finalizar, de lo contrario el proceso quedaria Zombie.
Por lo tanto, para que un proceso se ejecute en segundo plano, el proceso padre debe hacer un wait() a su hijo cada vez que se ejecute otro comando. Utilizando el flag WNOHANG de la syscall  waitpid(), se logra este comportamiento. El proceso padre espera a que su hijo finalice, pero mientras tanto, otros procesos son ejecutados en primer plano, siendo esperados por sus respectivos padres.

---

### Flujo estándar

El tipo de redireccion 2>&1 significa una redireccion de la salida de error estandar (STDERR) hacia el mismo lugar que la salida estandar (STDOUT).

En el ejemplo: <br>
ls -C /home /noexiste >out.txt 2>&1 <br>
El archivo out.txt se habra escrito con la salida del comando ls, donde al ejecutar con el argumento **/home** se escribe desde la salida estandar, mientras que al utilizar el argumento **/noexiste** (no existe ese directorio), el comando lanzara un error. Como la redireccion lo indica, la salida estandar de error se escribira en el mismo archivo que la salida estandar.
Al ejecutar: 

cat out.txt <br>
ls: cannot access '/noexiste': No such file or directory <br>
/home: <br>
rocioplatini <br>

Si invierto el orden de las redirecciones:

ls -C /home /noexiste 2>&1 >out.txt 
cat out.txt <br>
ls: cannot access '/noexiste': No such file or directory <br>
/home: <br>
rocioplatini <br>

La salida es la misma.

---

### Tuberías simples (pipes)
Cada comando tiene su propio exit code (status). Cada vez que finaliza el proceso asociado a ese comando, se ejecuta el *print_status* donde se muestra el exit code del proceso (0 en success u otro numero si hubo un error). Cuando se ejecutan pipes, el exit code reportado por la shell es el del ultimo comando ejecutado.

Utilizando bash:

Falla cat: <br>
cat noexiste | echo hola <br>
hola <br>
cat: noexiste: No such file or directory <br>
echo $? <br>
0 <br>

Falla grep: <br>
cat out.txt | grep hola <br>
echo $? <br>
1 <br>

Si utilizo la shell implementada en el lab, obtengo exactamente las mismas salidas.

---

### Pseudo-variables

## **$ !**
Muestra el ID del proceso del ultimo comando en segundo plano.

sleep 5& <br>
echo $! <br>
18560 <br>

## **$$** 
Muestra el ID del proceso de la shell actual.

echo $$ <br>
2554 <br>

## **$_** 
Muestra el ultimo argumento pasado al comando anterior

cat out.txt <br>
echo $_ <br>
out.txt <br>

---


