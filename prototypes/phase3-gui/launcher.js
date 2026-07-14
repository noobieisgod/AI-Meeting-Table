document.querySelectorAll('a.button').forEach((link) => link.addEventListener('click', () => sessionStorage.setItem('aimtConceptOpened', link.textContent)));
