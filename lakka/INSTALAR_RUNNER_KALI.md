# Compilar FITD-Lakka pelo Kali (runner próprio)

## Por que o workflow atual é lento?

O workflow anterior usa Ubuntu x86_64 do GitHub, mas executa toda a compilação
ARMHF por QEMU dentro de Docker. O processador do Kali NÃO é usado.

A nova opção executa o build pelo processador do próprio Kali, mantém uma
imagem Docker com os compiladores e usa ccache para reutilizar os objetos
que não mudaram. A primeira execução AINDA usa QEMU dentro do Kali; esta
não é, por enquanto, uma cross-compilação nativa x86->ARMHF. O ganho de
velocidade deve ser mais perceptível nas recompilações.

## Etapa 1 - preparar o Kali

    sudo apt update
    sudo apt install -y docker.io qemu-user-static binfmt-support curl python3 git tar unzip
    sudo systemctl enable --now docker
    sudo usermod -aG docker smwrunner

Se a VM não tiver a conta smwrunner, substitua pelo usuário normal que executará
este novo runner (não use root para registrar o runner). Como o runner do
Mario World está instalado para smwrunner, essa conta normalmente existe.

## Etapa 2 - registrar um runner SEPARADO para o projeto FITD

O runner existente para Super Mario World está vinculado ao repositório
do Mario; ele não é automaticamente utilizável pelo repositório FITD.

Acesse o link abaixo no navegador, com sua conta GitHub:

https://github.com/dehalbuq/FITD-R36S/settings/actions/runners/new

Escolha Linux / x64. O token de registro exibido é privado e temporário.
Não o envie para conversas.

No Kali:

    sudo -iu smwrunner
    git clone -b experimental/lakka-rk322x-gles2 https://github.com/dehalbuq/FITD-R36S.git ~/fitd-lakka-setup
    bash ~/fitd-lakka-setup/lakka/instalar_runner_kali.sh

O instalador baixa o pacote Linux x64 oficial, verifica SHA-256 fornecido pelo
GitHub e pede o token APENAS no terminal Kali. Ele registra o runner com a
label fitd-lakka-kali em ~/actions-runner-fitd-lakka.

Saia da conta smwrunner:

    exit

Como usuário com sudo, instale o serviço:

    sudo /home/smwrunner/actions-runner-fitd-lakka/svc.sh install smwrunner
    sudo /home/smwrunner/actions-runner-fitd-lakka/svc.sh start
    sudo /home/smwrunner/actions-runner-fitd-lakka/svc.sh status

Se houver erro de Docker sem permissão, depois de adicionar o usuário ao grupo:

    sudo /home/smwrunner/actions-runner-fitd-lakka/svc.sh stop
    sudo /home/smwrunner/actions-runner-fitd-lakka/svc.sh start
    sudo -u smwrunner docker info

O novo runner NÃO substitui nem interfere com o antigo runner do SMW.

## Etapa 3 - iniciar a compilação manualmente

Abra:

https://github.com/dehalbuq/FITD-R36S/actions/workflows/fitd-lakka-kali-selfhosted.yml

Clique Run workflow e selecione a branch experimental/lakka-rk322x-gles2.

Na tela do Kali, acompanhe com:

    htop
    docker stats

A primeira execução precisa montar a imagem Docker ARMHF, instalar
dependências e clonar FITD com submódulos. O processo depende do seu
acesso à internet e pode continuar lento durante esta etapa.

Nas execuções seguintes, a imagem de compilação, os fontes do FITD e
o cache de objetos serão reutilizados no Kali.

O workflow Kali é manual e exige a label fitd-lakka-kali; se o runner
não estiver online, o job ficará na fila. O workflow antigo emulado na
nuvem também passou a ser manual para não reiniciar a cada commit.

## Desempenho

Padrão: três compilações ARM em paralelo (FITD_BUILD_JOBS=3).
Para uma VM com CPU e RAM suficientes, pode aumentar para 4 alterando
FITD_BUILD_JOBS no workflow Kali. Em VM com pouca RAM, use 2.

OBSERVAÇÃO: Não afirme que QEMU foi removido. Esta etapa apenas transfere
a execução para o Kali e aproveita Docker layer cache e ccache. Para
eliminar QEMU será necessário um toolchain cross ARMHF com shaderc de
BGFX compilado para x86_64, etapa ainda não implementada.

## Segurança

O repositório FITD é público. Não permita execução de pull requests
de desconhecidos em runners self-hosted. O novo workflow é restrito a
execução MANUAL na branch experimental; não injeta tokens particulares
no contêiner. O usuário do runner com acesso ao Docker tem permissões
elevadas dentro da VM, por isso a instância deve ser dedicada à compilação.

O Lakka/TV Box não é alterado pelo registro deste runner.
O FITD ainda precisa compilar e passar no teste real no RK3229.
