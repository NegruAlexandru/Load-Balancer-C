**Nume: Negru Alexandru-Nicolae**

**Grupă: 314CAb**

## Distributed Database (Tema 2 SDA)

### Descriere:

* Programul reprezinta implementarea unui load balancer, impreuna cu implementarile de server si memorie cache de tip LRU in limbajul C.

### Utilizare
* #### Initierea programului
    Pe prima linie a input file-ului se va afla numarul de request-uri care vor urma, (optional) alaturi de "ENABLE_VNODES" in cazul in care se doreste utilizarea de noduri virtuale.

    Exemple:
    ```bash
    54 ENABLE_VNODES
    ```
    ```bash
    26
    ```

  #### Proces:
  * Programul contorizeaza numarul de request-uri facute si se opreste cand se atinge numarul utilizat in initiere.
  * De asemenea, in functie de optiunea aleasa legata de nodurile virtuale, se vor utiliza replici (noduri virtuale) ale server-elor sau nu. Lucru realizat prin crearea unor replici fiecarui server, care prin accesare conduc la baza de date/cache-ul server-ului original, astfel incat load-ul total va fi distribuit uniform.

* #### Load balancer
    Este posibila adaugarea si eliminarea server-elor.
    Adaugarea se va face impreuna cu precizarea dimensiunii cache-ului.

    Exemple:
    ```bash
    ADD_SERVER 58994 10
    ```

    ```bash
    REMOVE_SERVER 18963
    ```

  #### Proces:
  * Adaugarea se va efectua prin alocarea de memorie necesara pentru baza de date, cache si coada de request-uri. De asemenea, i se va asocia o pozitie pe hash ring astfel incat datele retinute in array-ul de server-e sa fie distribuite.
  * Eliminarea unui server presupune transferarea tuturor datelor retinute de acesta in urmatorul (urmatoarele server-e, in cazul utilizarii nodurilor virtuale), apoi eliberarea memoriei.


* #### Server
  Pentru a interactiona cu server-ele sunt disponibile comenzi de EDIT, pentru crearea sau modificarea unui document, si de GET pentru obtinerea continutului unui document.

  Exemple:
  ```bash
  EDIT "manager.txt" "Box understand feel."
  ```

  ```bash
  GET "manager.txt"
  ```

  #### Proces:
  * Modificarea unui document va consta in verificarea existentei acestuia si modificarea continutului acestuia, sau, in cazul in care nu exista, crearea acestuia. Ambele cazuri conduc la adaugarea sa in cache, fiind un document accesat recent.
  * Obtinerea continutului se va strict in cazul in care documentul exista, altfel utilizatorul este informat de faptul ca documentul nu exista. Daca este gasit, va fi adaugat in cache.

### Comentarii asupra temei:

* Crezi că ai fi putut realiza o implementare mai bună?

Sunt de parere ca intervalul scurt de timp pe care l-am avut disponibil pentru aceasta tema a dus la aparitia unor decizii slabe cand vine vorba de design si de decizii de implementare a conceptelor discutate in enunt. Cred ca as fi putut structura mai bine structura de date "entry" si operatiile din jurul acesteia incat sa sporesc eficienta programului. De asemenea, sunt constient ca programul actual prezinta cateva greseli de selectionare a server-elor/transferare a datelor, asupra carora nu am avut timp sa ma uit cu prea multa atentie. 

* Ce ai invățat din realizarea acestei teme?

Pot spune ca am invatat multe despre cum functioneaza cu adevarat o retea de server-e, starnindu-mi curiozitatea inspre cautarea a mai multe informatii pe aceasta directie. Totodata, utilizarea structurilor de date precum cozi (pentru coada de request-uri) sau hash maps (stocarea eficienta de date) m-a ajutat sa inteleg mai bine unele dintre aplicatiile lor in practica. 

