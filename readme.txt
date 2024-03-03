DON'T MAKE THE SAME MISTAKE! USE C++, NOT C!

! Am utilizat 2 sleep days

!! Pe local am passed pe toate testele din scriptul de testare, iar daca exista
vreo problema la rulare (daca nu trece de subscribe_all sau ceva), probabil
trebuie resetat host-ul pe care se ruleaza.
Mentionez ca am lucrat pe un VM cu Ubuntu 20.04 (64-bit)
Spun toate acestea deoarece chiar am stat mult pe tema si mie imi ruleaza ok

!!! Arhiva va contine, pe langa readme si Makefile, fisierele sursa server.c,
subscriber.c - acestea fiind cele cerute, dar si un helper.c unde am definit
un send si recv custom, alaturi de header-ul helper.h.

Timp de implementare: ~30h

Structuri si incapsulare:

Cand m-am apucat de tema, aveam de ales dintre 2 variate de abordare:
o varianta care se centreaza pe topic-uri si una care se centreaza
pe clienti, dar am ales-o pe care se centreaza pe clienti deoarece am considerat
ca elementul central al temei este optiunea de Store and Forward, fiind mai
clar de implementat asa, chiar daca cealalta probabil ar fi fost mai eficienta

Centrata pe clienti = clientii contin o lista cu topic-urile la care sunt
abonati, nu topic-urile contin link-uri spre clienti.

Vom lucra totusi cu 2 liste mari: una de topic-uri "all_topics" si una de clienti
"all_clients", definite in main.

Fiecare client (element de tip Tcp_client), contine datele generale despre clientul
respectiv, o lista de topic-uri la care este abonat (topic_element) si un link
catre urmatorul client. In main este definita lista de clienti "all_clients".

Un element de tip "Client_topic", parte a listei de topic-uri pt fiecare client.
contine un pointer spre Topic-ul pe care il identifica si datele clientului pt acel topic.

Asadar, nu facem o copie a topic-ului pt fiecare client care este abonat la el, ci
doar asociem un pointer spre Topic. Daca clientii C1 si C2 sunt amandoi conectati
la topic_a, atunci nu vor exista 2 structuri de tip Topic cu copii ale mesajelor,
ci 2 structuri de tip Client_topic, cu elementul "topic" pointand la acelasi Topic.

Structura de tip Topic:
Contine in sine o lista de Mesaje, avand un pointer la inceputul si la finalul listei.
Pointerul spre tail are rolul de a usura adaugarea elementului.
Campul no_messages_ever tine cont cate mesaje s-au trimis vreodata pe topic_ul respectiv,
iar un mesajele vor avea numere de ordine cresacatoare. Practic asa tin cont de acel
numar de ordine.

Structura de tip Message - incapsularea mesajelor:

identifier-> numarul de ordine al mesajului pe topic-ul respectiv (asa am invatat la
laborator ca se procedeaza la tcp - cu numere de ordine)
len-> lungimea mesajului in sine
message_value-> mesajul in sine ce urmeaza a fi parsat de catre client
no_possible_clients-> calculat cand primim de la udp mesajul
no_sent_clients-> la cati clienti am trimis so far
Daca ultimele 2 sunt egale => am trimis peste tot pe unde trebuia si putem sterge
mesajul din memoire (functia check_remove).

! Numarul de clienti nu este limitat (avand lista pentru structuri, iar vectorul
de pfds isi mareste dimensiunea daca ajunge la capacitate maxima - la reach)
! De asemenea, Mesajele si topicurile nu sunt limitate de ceva, fiind liste.

-Workflow general server:

Vom avea o lista de clienti si una de topic-uri.
sock_udp si udp_buffer - asociate conexiunii udp
sock_tcp_main - socketul pe care se da listen si se accepta conexiuni de clienti
tcp_buffer - buffer de trimitere/citire asociat tuturor conexiunilor tcp

pfds -> vector ce retine toate socket-urile din problema
nfds -> nr de socketuri pe care le vom baga in seama
Conectare client nou -> adaugam la final de vector un fd
Deconectare client -> facem interschimbare intre ultimul fd din vector si cel
care o sa se deconecteze si scadem nfds

Primire de la udp:
Extragem numele topic-ului din mesaj si adaugam la lista de mesaje asociata
topicului respectiv. Pentru a indeplini protocolul, trebuie sa alipim la
mesaj datele expeditorului si sa ii salvam si lungimea in structura Message.
Verificam dupa fiecare mesaj daca avem cui trimite si trimitem instant.

Primire stdin:
Actiune cu sens are loc doar daca se primeste exit de la tastatura.
O sa se trimita o notificare de inchidere la toti clientii si se vor inchide socketii.
Altfel o sa se afiseze pe ecran un mesaj de eroare.

Acceptare conexiuni:
Se verifica starea clientului (daca mai este deja conectat sau nu). IN cazul unui
eveniment neok, se trimite un mesaj catre client si eventual se inchide.
In cazul reconectarii unui client, se verifica daca avea topic-uri la care era
abonat cu sf si se trimit eventual mesaje.

Primire date de la clienti:
Explicat in sectiunea "PROTOCOL"


- Workflow general clienti:
Practic parsarea mesajelor se face in client
Un mesaj de la server se va citi folosind functia my_recv, care practic se asigura ca se
primesc nr de octeti pe care vrem sa il primim.
In functie de codul mesajului pe care il primim, facem parsarea corespunzator.

PROTOCOL / PROTOCOALE:

1. Conectare clienti:
In momentul in care un client vrea sa se conecteze la server si serverul da accept,
serverul va astepta de la client numele acestuia, iar clientul stie ca va trebui
sa il trimita pe acesta.

2. Trimitere de la client la server a mesajelor:
Practic tot ce primeste clientul de la atstatura se va trimite catre server, 
acesta din urma avand rolul de a decide daca mesajul este valid si ce sa faca cu el.
Daca mesajul este "exit" -> subscriberul se va inchide, iar serverul stie sa inchida
si el conexiunea si socketul.

"subscribe" -> serverul analizeaza daca clientul se poate conecta, caz in care trimite
un fel de ACK (da, se poate) sau NACK (e deja conectat), sub forma de mesaje pe care
clientul sa le poate afisa la stdout.

"unsubscribe" -> cam la fel ca la subscribe, dar aici am implementat eu mai multe tipuri
de mesaje de eroare, in functie de topicul la care se face referire.

"exit" -> clientul notifica serverul ca urmeaza sa se inchida, nu mai asteapta raspuns

3. Trimitere de mesaje de la server la client:
Pratcic protocolul cel mai relevant din toata tema, in fiecare structura de tip Message,
noi retinem lungimea mesajului si un char*. Acel char* reprezinta mesajul pe care
clientul stie sa il interpreteze.

Interpretarea mesajului:
Parsarea se face complet in client. Este mai rapid pentru server sa incapsuleze mesajele
asa cum sunt primite si sa le dea asa mai departe.
La inceput este serializat un struct sockaddr_in cu datele clientului de udp,
apoi datele in sine. Clientul stie ca mai intai trebuie sa citeasca un sockaddr_in,
apoi sa desparta mesajul dupa formatul din cerinta.

Trimitere / receptionare mesaj:
Intotdeauna se trimite intai lungimea, apoi mesajul in sine. Am salvat lungimea intr-un
short deoarece chiar si o retea nesigura poate trimite totusi 2 bytes corect.
Receptionarea in client se face intai pentru lungime, apoi se citesc octeti pana
suntem siguri ca s-a primit nr bun de bytes.

Functiile my_send si my_recv se afla in fisierul helper.c

Concluzie:
Eu zic ca am respectat toate cerintele temei si am incercat sa realizez o forma de
incapsulare a mesajelor.
De asemenea, am incercat sa implementez cat mai multe notiuni invatate la laboratoarele
de TCP, cum ar fi: pastrarea a unui numar de ordine pentru fiecare mesaj sau trimiterea
unui ACK / NACK macar pentru partea de subscribe si unsubscribe, dar si trimiterea
unei notificari de genul; "Ma inchid" in momentul in care se da comanda exit.
