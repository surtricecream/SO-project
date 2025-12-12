# PASSO: comando de espaçamento de movimentos
# Só existe um no início por ficheiro. Indica quantas jogadas esperar
# entre cada movimento.
PASSO 1
# POS: comando de colocação inicial do monstro (linha e coluna).
# Assume-se que não é possível o monstro ser colocado
# numa posição impossível/inexistente.
# Só existe um no início por ficheiro.
POS 3 3
# Todos os comandos após PASSO e POS são executados em ciclo infinito.
# Os comandos possíveis são A (esq.), D (dir.), W (cima.), S (baixo)
# R (direcção aleatória), T (espera um número de jogadas), C (carregar)
T2
A
A
D
T2
W
S
S
S