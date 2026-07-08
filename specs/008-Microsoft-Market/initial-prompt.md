OBJECTIF PRINCIPAL  
Intégrer dans le projet toutes les étapes, fichiers, scripts et configurations nécessaires pour permettre la publication de l’application sur le Microsoft Store en utilisant le chemin le moins coûteux, c’est‑à‑dire un packaging MSIX avec signature automatique et hébergement gratuit via le Store.

CONTRAINTES ET PRINCIPES À RESPECTER

Aucun certificat de signature payant

Utiliser exclusivement la signature automatique fournie par MSIX Packaging Tool ou par Visual Studio.

Ne pas intégrer de certificat tiers (.pfx) nécessitant un achat.

Packaging MSIX obligatoire

Créer un profil de packaging MSIX pour l’application.

Générer un AppxManifest.xml complet et conforme aux exigences du Microsoft Store.

Définir les capacités (capabilities) minimales nécessaires.

Définir les déclarations (extensions) nécessaires selon les fonctionnalités de l’app.

Structure du projet à adapter

Ajouter un dossier StorePackaging/ contenant :

Le fichier .msixproj

Le manifest

Les assets (icônes, logos, splash si nécessaire)

Les scripts de build

Vérifier que le projet principal peut être empaqueté sans modification majeure.

Automatisation du build MSIX

Ajouter un script PowerShell ou un workflow CI (GitHub Actions ou autre) permettant :

Build de l’application

Packaging MSIX

Génération du package final .msixbundle

Le script doit fonctionner sans certificat externe.

Préparation pour Partner Center

Générer les éléments nécessaires à la soumission :

Numéro de version cohérent (ex : 1.0.0.0)

Icônes aux formats requis (Store : 44×44, 50×50, 150×150, 310×310, etc.)

Description technique (capabilities, architecture, dépendances)

Validation locale avant soumission

Intégrer un script ou une procédure permettant de tester le package MSIX localement.

Vérifier que l’installation fonctionne sans privilèges administrateur.

Vérifier que le package passe les tests de base du Store (API usage, stabilité).

LIVRABLES ATTENDUS

Un dossier complet StorePackaging/ prêt à être utilisé.

Un fichier .msixproj fonctionnel.

Un manifest MSIX valide et optimisé.

Les assets graphiques aux formats requis.

Un script de build automatisé (PowerShell ou CI).

Une documentation interne expliquant :

Comment générer le MSIX

Comment tester le package

Comment soumettre le package dans Partner Center

Les erreurs courantes de certification et comment les éviter

BUT FINAL