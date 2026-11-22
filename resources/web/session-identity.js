(() => {
    const site = 'https://archiveofourown.org';
    const greeting = document.getElementById('greeting');
    if (!greeting || greeting.closest('#workskin, .userstuff')) return '';
    const url = (element, attribute) => {
        try {
            const value = new URL(element.getAttribute(attribute), site);
            return value.origin === site && !value.username && !value.password ? value : null;
        } catch (_) { return null; }
    };
    const logout = Array.from(greeting.querySelectorAll('a[href], form[action]')).some(element => {
        const target = url(element, element.tagName === 'FORM' ? 'action' : 'href');
        return target && target.pathname === '/users/logout';
    });
    if (!logout) return '';
    for (const link of greeting.querySelectorAll('a[href]')) {
        const target = url(link, 'href');
        const match = target && target.pathname.match(/^\/users\/([A-Za-z0-9_]+)$/);
        if (match && match[1] !== 'login' && match[1] !== 'logout') return match[1];
    }
    return '';
})()
