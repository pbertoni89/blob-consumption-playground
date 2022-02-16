#!/usr/bin/python3-xnext
# /usr/bin/env python3

import pandas
import os
import re
import numpy
from matplotlib import pyplot as plt


elfs, mrks = ['pool', 'async'], ['x', '+']


def _scatter_colour_elf(df: pandas.DataFrame, elf: str, marker: str, xcol: str, ycol: str, cat_col: str, ax_curr):
    df_e = df.loc[df['i_elf'] == elf]
    categories = numpy.unique(df_e[cat_col])
    ncat = len(categories)
    # colours_f = numpy.linspace(0, 1, ncat)
    colours_f = list('bgrcmk')
    if len(colours_f) < ncat:
        raise NotImplementedError('cycle on categories')  # TODO
    else:
        colours_f = colours_f[:ncat]
    colour_d = dict(zip(categories, colours_f))
    # ser_colour = df_e[cat_col].apply(lambda x: colour_d[x])

    for cat, col in zip(categories, colours_f):
        df_e_j = df_e.loc[df_e['i_n_jobs'] == cat]
        # ser_this_col = ser_colour.loc[ser_colour == col]
        # print(f'elf {elf}, marker {marker}, cat {cat}, col {col}, ser {ser_this_col}\n')
        # ax_curr.scatter(df_e_j[xcol], df_e_j[ycol], c=ser_this_col, marker=marker, label=cat)
        ax_curr.scatter(df_e_j[xcol], df_e_j[ycol], c=colour_d[cat], marker=marker, label=cat)


def _scatter_colour(df: pandas.DataFrame, xcol: str, ycol: str, cat_col: str, ax_curr, title=None) -> None:
    for elf, marker in zip(elfs, mrks):
        _scatter_colour_elf(df, elf, marker, xcol, ycol, cat_col, ax_curr)
    plt.xlabel(xcol)
    plt.ylabel(ycol)
    plt.legend()
    title = '' if title is None else title
    plt.title(title)


def _plot(results_dir: str, df: pandas.DataFrame) -> None:
    fn_out = os.path.join(results_dir, 'analysis')
    print(df.to_string())
    df.to_csv(fn_out + '.csv')

    fig, ax = plt.subplots(nrows=2, ncols=2)
    sup_title = os.path.split(results_dir)[-1] + '. ' + ', '.join([f'{e}_{m}' for e, m in zip(elfs, mrks)])
    fig.suptitle(sup_title)
    ax_curr = plt.subplot(2, 2, 1)
    _scatter_colour(df, xcol='po_extra_time_s', ycol='pi_load', cat_col='i_n_jobs', ax_curr=ax_curr)
    ax_curr = plt.subplot(2, 2, 2)
    _scatter_colour(df, xcol='o_act_time_s', ycol='o_exp_time_s', cat_col='i_n_jobs', ax_curr=ax_curr)
    ax_curr = plt.subplot(2, 2, 3)
    # _scatter_colour(df, xcol='po_extra_time_s', ycol='o_max_miss', cat_col='i_n_jobs', ax_curr=ax_curr)
    _scatter_colour(df, xcol='po_extra_time_s', ycol='o_max_task', cat_col='i_n_jobs', ax_curr=ax_curr)
    ax_curr = plt.subplot(2, 2, 4)
    _scatter_colour(df, xcol='o_max_task', ycol='pi_load', cat_col='i_n_jobs', ax_curr=ax_curr)

    plt.savefig(fn_out + '.png')


def _parse_results(results_dir: str, only_longest_run: bool) -> pandas.DataFrame:
    re_cmd = re.compile(r'.*--elf (\w+) --inms (\d+) --outms (\d+) --njobs (\d+) --debug \d --nblobs (\d+)')
    re_alpha = re.compile(r'.*alpha.*-> (\d+).*-> (\d+).*\[s] (\d+)')
    re_task_miss = re.compile(r'.*MaxTaskEver (\d+).*MaxMissEver (\d+)')
    re_omega = re.compile(r'.*omega.*elapsed \[s] (\d+).*')
    dd = {'i_elf': [], 'i_in_ms': [], 'i_out_ms': [], 'i_n_jobs': [], 'i_n_blobs': [],
          'o_max_task': [], 'o_max_miss': [],
          'o_in_exp_rate': [], 'o_out_exp_rate': [], 'o_exp_time_s': [], 'o_act_time_s': []}
    df = pandas.DataFrame(dd)

    with open(os.path.join(results_dir, 'results.txt'), 'r') as fo:
        elf, inms, outms, njobs, nblobs, mtask, mmiss, exp_rate_i, exp_rate_o, exp_time_s, act_time_s = \
            (None for _ in range(11))
        for line in fo.readlines():
            ln = line.strip()
            m_cmd = re_cmd.match(ln)
            if m_cmd:
                elf, inms, outms, njobs, nblobs = m_cmd.groups()
                # print(f'C {m_cmd.groups()}')
                continue
            m_alpha = re_alpha.match(ln)
            if m_alpha:
                exp_rate_i, exp_rate_o, exp_time_s = m_alpha.groups()
                # print(f'A {m_alpha.groups()}')
                continue
            m_task_miss = re_task_miss.match(ln)
            if m_task_miss:
                mtask, mmiss = m_task_miss.groups()
                continue
            m_omega = re_omega.match(ln)
            if m_omega:
                act_time_s, = m_omega.groups()
                # print(f'O {m_omega.groups()}')
                ls_row = [elf, inms, outms, njobs, nblobs, mtask, mmiss, exp_rate_i, exp_rate_o, exp_time_s, act_time_s]
                row = pandas.Series(ls_row, index=df.columns)
                # print(f'appending {row}')
                df = df.append(row, ignore_index=True)
                continue

    if only_longest_run:
        max_nblobs = df['i_n_blobs'].max()
        print(f'********************* only the longest run: {max_nblobs}')
        df = df[df['i_n_blobs'] == max_nblobs]
    return df


def _postprocessing(df: pandas.DataFrame) -> pandas.DataFrame:
    dtypes = {d: int for d in ['i_in_ms', 'i_out_ms', 'i_n_jobs', 'i_n_blobs', 'o_max_task', 'o_max_miss',
                               'o_in_exp_rate', 'o_out_exp_rate', 'o_exp_time_s', 'o_act_time_s']}
    dtypes['i_elf'] = 'category'
    df = df.astype(dtypes)

    n_jobs_max = df['i_n_jobs'].max()  # shall be equal to NUM_CORES

    def check_async_multicore(fixed: bool):
        df_async = df.loc[df['i_elf'] == 'async']
        df_async_multicore = df.loc[(df['i_n_jobs'] > 1) & (df['i_elf'] == 'async')]
        nasync_multicore = len(df_async_multicore)
        nasync = len(df_async)
        if nasync_multicore > 0 and not fixed:
            raise ValueError(f'async was marked 1 n_jobs, not {n_jobs_max}: {nasync_multicore}')
        elif nasync_multicore != nasync and fixed:
            raise ValueError(f'async is marked {n_jobs_max} n_jobs, not {nasync}: {nasync_multicore}')

    check_async_multicore(False)

    df.loc[df['i_elf'] == 'async', 'i_n_jobs'] = n_jobs_max

    # FIXME DUNNO WHY BUT IT WORKS AND it even shows us why `async` could be better
    magic = True
    # in facts, exp_time_s estimation (done in C++) is wrong for `async` elf, because it internally
    # pretends to work on a single job, therefore messing up with `pool` estimation. However, act_time_s clearly
    # tells us that `async` is always on time (at least with these parameters choice, hardware included).
    # the fun fact here is that n_blobs = 1000 and this brings an identity between [ms] and [s] times
    if magic:
        df.loc[df['i_elf'] == 'async', 'o_exp_time_s'] = \
            df.loc[df['i_elf'] == 'async', 'i_in_ms'] * (df.loc[df['i_elf'] == 'async', 'i_n_blobs'] / 1000)

    check_async_multicore(True)

    df['po_extra_time_s'] = (df['o_act_time_s'] - df['o_exp_time_s']).astype(int)

    df['pi_load'] = (df['i_out_ms'] / df['i_n_jobs']) / df['i_in_ms']

    df = df.astype(dtypes)  # re-cast after any edit and before dumping
    return df


def main(results_dir: str):
    print(f'running for data directory {results_dir}')
    df = _parse_results(results_dir, only_longest_run=True)
    df = _postprocessing(df)
    _plot(results_dir, df)


if __name__ == '__main__':
    datas = [ll for ll in [x[0] for x in os.walk('.')] if ll.startswith('./data-')]
    do_show = True
    for data in datas:
        path_to_dir = os.path.join(os.path.dirname(__file__), data)
        main(path_to_dir)
    if do_show:
        plt.show()
